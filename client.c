#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <endian.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <assert.h>
#include "cs341proj.h"

char *exec_cmd;				/* Command line by execution. */

char *host;					/* Host address. */
char *port;					/* Port number. */
int operation;				/* Operation. */
char keyword[MAX_KEY + 1];	/* Keyword. */

uint8_t buf[MAX_MSG];		/* Message buffer. */

int ddebug;					/* Debug mode. */

/* Parse the command line. */
static void parse_command(int argc, char **argv)
{
	int opt;
	host = NULL;
	port = NULL;

	while ((opt = getopt(argc, argv, "h:p:o:k:d")) != -1) {
		switch (opt) {
		case 'h':
			if ((host = (char *) malloc((strlen(optarg) + 1) * sizeof(char))) == NULL) {
				eprintf("malloc error: %s\n", strerror(errno));
				exit(3);
			}
			strcpy(host, optarg);
			break;
		case 'p':
			if ((port = (char *) malloc((strlen(optarg) + 1) * sizeof(char))) == NULL) {
				eprintf("malloc error: %s\n", strerror(errno));
				exit(3);
			}
			strcpy(port, optarg);
			break;
		case 'o':
			if (sscanf(optarg, "%d", &operation) != 1 
				|| (operation != 0 && operation != 1)) {
				eprintf("bad operation.\n");
				exit(2);
			}
			break;
		case 'k':
			if (strlen(optarg) > MAX_KEY) {
				eprintf("keyword too long.\n");
				exit(2);
			}
			strcpy(keyword, optarg);
			break;
		case 'd':
			ddebug = 1;
			break;
		default:
			/* An error occurred. We exit program in this case. */
			eprintf("usage: %s [-h host] [-p port] [-o operation] [-k keyword]\n", argv[0]);
			exit(2);
		}
	}
}

/* Setup the socket. Returns the socket file descriptor. */
static int setup_client(void)
{
	struct addrinfo hints, *res, *it;
	int ret;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_NUMERICSERV | AI_ADDRCONFIG;
	if ((ret = getaddrinfo(host, port == NULL ? DEFAULT_PORT : port, &hints, &res)) != 0) {
		eprintf("getaddrinfo: %s\n", gai_strerror(ret));
		exit(1);
	}

	for (it = res; it != NULL; it = it->ai_next) {
		if ((ret = socket(it->ai_family, it->ai_socktype, it->ai_protocol)) == -1)
			continue;

		if (connect(ret, it->ai_addr, it->ai_addrlen) != -1)
			break;

		close(ret);
	}

	if (it == NULL) {
		eprintf("failed to connect %s: %s\n",
			host == NULL ? "localhost" : host, strerror(errno));
		exit(1);
	}
	freeaddrinfo(res);
	return ret;
}

/* Communicate with server. */
static void start_client(int sockfd)
{
	size_t len;

	*((uint16_t *) buf) = htons((uint16_t) operation);
	memcpy(buf + KEY_OFS, keyword, MAX_KEY);

	while ((len = read(STDIN_FILENO, buf + DATA_OFS, MAX_MSG - DATA_OFS)) > 0) {

		len += DATA_OFS;
		*((uint64_t *) (buf + LEN_OFS)) = htobe64((uint64_t) len);
		*((uint16_t *) (buf + CHKSUM_OFS)) = ~checksum(buf, len);

		ssize_t write_len = write_packet(sockfd, buf, len);

		if (write_len == -1) {
			eprintf("write error: %s\n", strerror(errno));
			break;
		} else if (write_len == 0) {
			eprintf("lost connection\n");
			break;
		}
		
		ssize_t read_len = read_packet(sockfd, buf, MAX_MSG);

		if (read_len < 0) {
			eprintf("read error: %s\n", strerror(errno));
			break;
		} else if (read_len == 0) {
			eprintf("lost connection\n");
			break;
		}

		if (checksum(buf, read_len) != 0xffff) {
			eprintf("checksum error\n");
			break;
		}

		write(STDOUT_FILENO, buf + DATA_OFS, read_len - DATA_OFS);

		/* Set checksum field to zero. */
		memset(buf + CHKSUM_OFS, 0, KEY_OFS - CHKSUM_OFS);
	}

	close(sockfd);
}

/* Main client function. */
int main(int argc, char *argv[])
{
	/* Copy ARGV[0] to EXEC_CMD. */
	copy_exec_cmd(argv[0]);
	parse_command(argc, argv);
	start_client(setup_client());
	return 0;
}
