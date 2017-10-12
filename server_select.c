#include "server_common.c"
#include <sys/select.h>

/* Buffer for each file descriptor. */
enum sock_io {
	SOCK_READ,
	SOCK_WRITE
};

struct buffer {
	uint8_t *base;
	size_t read_ofs;
	size_t write_ofs;
	enum sock_io status;
} buf[FD_SETSIZE];

/* Add client to the FD_SET. Return 0 if success, -1 otherwise. */
static int add_client(int fd, fd_set *fds)
{
	FD_SET(fd, fds);

	if ((buf[fd].base = (uint8_t *) malloc(MAX_MSG)) == NULL)
		return -1;

	buf[fd].read_ofs = 0;
	buf[fd].status = SOCK_READ;

	return 0;
}

/* Remove client from the list. */
static void remove_client(int fd, fd_set *fds)
{
	FD_CLR(fd, fds);

	free(buf[fd].base);
}

/* Communicate with client. */
static void start_server(int sockfd)
{
	struct sockaddr addr;
	socklen_t addrlen;

	/* Preparation for SELECT. */
	int fd;
	fd_set fds, readfds, writefds;
	FD_ZERO(&fds);
	int fd_max = sockfd;
	int fd_num;

	for (;;) {
		writefds = fds;
		readfds = fds;
		FD_SET(sockfd, &readfds);
		if ((fd_num = select(fd_max + 1, &readfds, &writefds, NULL, NULL)) == -1) {
			if (errno == EINTR)
				continue;
			break;
		}

		for (fd = 0; fd <= fd_max && fd_num > 0; fd++) {
			if (FD_ISSET(fd, &readfds)) {
				fd_num--;
				if (fd == sockfd) {
					int connfd = accept(fd, &addr, &addrlen);
					if (connfd == -1) {
						eprintf("accept error: %s\n", strerror(errno));
					} else {
						if (add_client(connfd, &fds) == -1) {
							eprintf("malloc error: %s\n", strerror(errno));
							continue;
						}

						if (fd_max < connfd)
							fd_max = connfd;
					}
				} else {
					if (buf[fd].status == SOCK_READ) {
						uint8_t *base = buf[fd].base;
						size_t ofs = buf[fd].read_ofs;
						ssize_t read_len = read(fd, base + ofs, MAX_MSG - ofs);
						if (read_len == -1) {
							if (errno == EINTR)
								continue;
							eprintf("read error: %s\n", strerror(errno));
							remove_client(fd, &fds);
							continue;
						} else if (read_len == 0) {
							eprintf("lost connection\n");
							remove_client(fd, &fds);
							continue;
						}
						ofs += read_len;

						if (ofs < DATA_OFS) {
							buf[fd].read_ofs = ofs;
							continue;
						}

						size_t len = (size_t) be64toh(*((uint64_t *) (base + LEN_OFS)));
						if (len == ofs) {
							if (checksum(base, len) != 0xffff) {
								eprintf("checksum error\n");
								remove_client(fd, &fds);
								continue;
							}

							int operation = (int) ntohs(*((uint16_t *) (base + OP_OFS)));
							char *keyword = (char *) (base + KEY_OFS);
							char *key_end = (char *) (base + LEN_OFS);
							char *data = (char *) (base + DATA_OFS);
							size_t data_len = len - DATA_OFS;
							cipher(operation, keyword, key_end, data, data_len);

							memset(base + CHKSUM_OFS, 0, KEY_OFS - CHKSUM_OFS);
							*((uint16_t *) (base + CHKSUM_OFS)) = ~checksum(base, len);

							buf[fd].read_ofs = ofs;
							buf[fd].write_ofs = 0;
							buf[fd].status = SOCK_WRITE;
						} else if (len < ofs) {
							eprintf("length error\n");
							remove_client(fd, &fds);
							continue;
						} else {
							buf[fd].read_ofs = ofs;
							continue;
						}
					}
				}
			}

			if (FD_ISSET(fd, &writefds)) {
				fd_num--;
				if (buf[fd].status == SOCK_WRITE) {
					uint8_t *base = buf[fd].base;
					size_t ofs = buf[fd].write_ofs;
					size_t len = buf[fd].read_ofs;
					ssize_t write_len = write(fd, base + ofs, len - ofs);
					if (write_len == -1) {
						if (errno == EINTR)
							break;
						eprintf("write error: %s\n", strerror(errno));
						remove_client(fd, &fds);
						continue;
					} else if (write_len == 0) {
						eprintf("Lost connection\n");
						remove_client(fd, &fds);
						continue;
					}
					ofs += write_len;

					if (ofs == len) {
						buf[fd].read_ofs = 0;
						buf[fd].status = SOCK_READ;
					} else {
						buf[fd].write_ofs = ofs;
					}
				}
			}
		}
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
