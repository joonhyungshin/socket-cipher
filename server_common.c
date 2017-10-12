#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>
#include <getopt.h>
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
#include <ctype.h>
#include "cs341proj.h"

char *exec_cmd;				/* Command line by execution. */

char *port;					/* Port number. */

int ddebug;					/* Debug mode. */

/* Parse the command line. */
static void parse_command(int argc, char **argv)
{
	int opt;
	port = NULL;

	while ((opt = getopt(argc, argv, "p:d")) != -1) {
		switch (opt) {
		case 'p':
			port = (char *) malloc((strlen(optarg) + 1) * sizeof(char));
			strcpy(port, optarg);
			break;
		case 'd':
			ddebug = 1;
			break;
		default:
			/* An error occurred. We exit program in this case. */
			eprintf("usage: %s [-p port] [-d]\n", argv[0]);
			exit(2);
		}
	}
}

/* Reap all the child processes. */
static void reap_child(int signum)
{
	pid_t pid;
	for (;;) {
		pid = waitpid(-1, NULL, WNOHANG);
		if (pid <= 0)
			break;
	}
}
/* Register child reaper. */
static void handler_register(void)
{
	struct sigaction act;
	act.sa_handler = reap_child;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	if (sigaction(SIGCHLD, &act, NULL) == -1)
		eprintf("zombie reaper: %s\n", strerror(errno));
}

/* Setup the socket. Returns the socket file descriptor. */
static int setup_server(void)
{
	struct addrinfo hints, *res, *it;
	int ret, optval = 1;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV | AI_ADDRCONFIG;
	if ((ret = getaddrinfo(NULL, port == NULL ? DEFAULT_PORT : port, &hints, &res)) != 0) {
		eprintf("getaddrinfo: %s\n", gai_strerror(ret));
		exit(1);
	}

	for (it = res; it != NULL; it = it->ai_next) {
		if ((ret = socket(it->ai_family, it->ai_socktype, it->ai_protocol)) == -1)
			continue;

		if (setsockopt(ret, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int)) == 0
			&& bind(ret, it->ai_addr, it->ai_addrlen) == 0)
			break;

		close(ret);
	}
	
	freeaddrinfo(res);

	if (it == NULL) {
		eprintf("bind error: %s\n", strerror(errno));
		exit(1);
	}

	if (listen(ret, BACKLOG) != 0) {
		eprintf("listen error: %s\n", strerror(errno));
	}
	return ret;
}

static inline int is_alphabet(char c) {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

/* Cipher code. */
static void cipher(int op, char *key_start, char *key_end, char *data, size_t len) {
	char *keyword = key_start;
	if (!is_alphabet(*keyword))
		return;

	while (len > 0) {
		if (is_alphabet(*data)) {
			int ch;
			if (op == 0) {
				ch = tolower(*data) + (tolower(*keyword) - 'a');
				if (ch > 'z')
					ch -= 26;
			} else {
				ch = tolower(*data) - (tolower(*keyword) - 'a');
				if (ch < 'a')
					ch += 26;
			}
			*data = ch;

			keyword++;
			if (keyword >= key_end || !is_alphabet(*keyword))
				keyword = key_start;
		}

		len -= sizeof(char);
		data++;
	}
}
