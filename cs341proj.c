#include "cs341proj.h"
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <endian.h>
#include <errno.h>

/* Useful to print error messages. */
int eprintf(const char *format, ...)
{
	int ret = 0;
	if (ddebug) {
		va_list args;

		va_start(args, format);
		fprintf(stderr, "%s: ", exec_cmd);
		ret = vfprintf(stderr, format, args);
		va_end(args);
	}

	return ret;
}

/* Copy execution command. */
void copy_exec_cmd(char *cmd)
{
	exec_cmd = cmd;
}

/* Calculate the checksum. This should be called after all other
   bytes are set. */
uint16_t checksum(void *msg, size_t len)
{
	uint8_t *base = (uint8_t *) msg;
	uint8_t *end = (uint8_t *) msg + len;
	uint8_t *ptr;
	uint64_t res = 0;

	/* This is to guarantee that the checksum value is not messed up
	   by some unwanted value. */
	if (len & 1)
		*end = 0;

	for (ptr = base; ptr < end; ptr += 2) {
		res += (uint64_t) *((uint16_t *) ptr);
	}
	while (res >> 16) {
		uint64_t tmp = res >> 16;
		res &= (uint64_t) 0xffff;
		res += tmp;
	}

	return (uint16_t) res;
}

/* Robustly read maxmimum COUNT bytes. 
   Returns -1 if error occurred. */
ssize_t read_packet(int fd, void *buf, size_t count)
{
	if (count == 0)
		return 0;

	uint8_t *base = buf;
	uint8_t *ptr = buf;
	ssize_t read_len = 0;

	while (ptr < base + DATA_OFS && count > 0) {
		ssize_t n = read(fd, ptr, count);

		if (n == -1) {
			if (errno == EINTR)
				continue;
			return -1;
		} else if (n == 0) {
			return 0;
		}

		read_len += n;
		ptr += n;
		count -= n;
	}

	size_t len = (size_t) be64toh(*((uint64_t *) (base + LEN_OFS)));
	while (ptr < base + len && count > 0) {
		ssize_t n = read(fd, ptr, count);

		if (n == -1) {
			if (errno == EINTR)
				continue;
			return -1;
		} else if (n == 0) {
			return 0;
		}

		read_len += n;
		ptr += n;
		count -= n;
	}

	/* If the length is different, then the protocol went wrong. */
	return read_len == len ? read_len : 0;
}

/* Robustly write exactly COUNT bytes.
   Returns -1 if error occurred. */
ssize_t write_packet(int fd, void *buf, size_t count)
{
	uint8_t *ptr = buf;
	ssize_t write_len = 0;

	while (count > 0) {
		ssize_t n = write(fd, ptr, count);

		if (n == -1) {
			if (errno == EINTR)
				continue;
			return -1;
		} else if (n == 0) {
			return 0;
		}

		write_len += n;
		ptr += n;
		count -= n;
	}

	return write_len;
}

/* Dump the given pointer. BUF should be 64-bit aligned. */
void hex_dump(void *buf)
{
	uint8_t *base = (uint8_t *) buf;
	for (int i = 0; i < 8; i++) {
		fprintf(stderr, "%02x", *(base + i));
	}
	fprintf(stderr, "\n");
	base += LEN_OFS;
	for (int i = 0; i < 8; i++) {
		fprintf(stderr, "%02x", *(base + i));
	}
	fprintf(stderr, "\n");
}
