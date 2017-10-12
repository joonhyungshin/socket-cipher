#include "server_common.c"

uint8_t buf[MAX_MSG];		/* Message buffer. */

/* Communicate with client. */
static void start_server(int sockfd)
{
	struct sockaddr addr;
	socklen_t addrlen;
	pid_t child;

	for (;;) {
		int connfd = accept(sockfd, &addr, &addrlen);
		if (connfd == -1)
			continue;

		child = fork();
		if (child == -1) {
			eprintf("fork error: %s\n", strerror(errno));
		} else if (child == 0) {
			ssize_t read_len;
			while ((read_len = read_packet(connfd, buf, MAX_MSG)) > 0) {
				if (checksum(buf, read_len) != 0xffff) {
					eprintf("checksum error\n");
					break;
				}

				int operation = (int) ntohs(*((uint16_t *) (buf + OP_OFS)));
				char *keyword = (char *) (buf + KEY_OFS);
				char *key_end = (char *) (buf + LEN_OFS);
				char *data = (char *) (buf + DATA_OFS);
				size_t len = read_len - DATA_OFS;
				cipher(operation, keyword, key_end, data, len);

				memset(buf + CHKSUM_OFS, 0, KEY_OFS - CHKSUM_OFS);
				*((uint16_t *) (buf + CHKSUM_OFS)) = ~checksum(buf, read_len);

				ssize_t write_len = write_packet(connfd, buf, read_len);
				if (write_len == -1) {
					eprintf("write error: %s\n", strerror(errno));
					break;
				} else if (write_len == 0) {
					eprintf("lost connection\n");
					break;
				}

				memset(buf, 0, sizeof(buf));
			}

			if (read_len == -1) {
				eprintf("read error: %s\n", strerror(errno));
				exit(1);
			}
			exit(0);
		}

		/* Close the connection. */
		close(connfd);
	}

	eprintf("server closed: %s\n", strerror(errno));
	close(sockfd);
}

/* Main server function. */
int main(int argc, char **argv)
{
	copy_exec_cmd(argv[0]);
	parse_command(argc, argv);
	handler_register();
	start_server(setup_server());
	return 0;
}
