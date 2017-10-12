#ifndef CS341PROJ_H
#define CS341PROJ_H

#include <sys/types.h>
#include <stdint.h>

/* This is a maximum byte of each message. */
#define MAX_MSG 10000000
#define MAX_KEY 4

/* Default port number to use. */
#define DEFAULT_PORT "8000"

/* Protocol information. Offsets in bytes. */
#define OP_OFS 0
#define CHKSUM_OFS 2
#define KEY_OFS 4
#define LEN_OFS 8
#define DATA_OFS 16

/* Maximum capactiy of server queue. */
#define BACKLOG 16

extern char *exec_cmd;

extern int ddebug;

int eprintf(const char *format, ...);

void copy_exec_cmd(char *cmd);

uint16_t checksum(void *msg, size_t len);

ssize_t read_packet(int fd, void *buf, size_t count);
ssize_t write_packet(int fd, void *buf, size_t count);

/* For debugging purpose. */
void hex_dump(void *buf);

#endif /* cs341proj.h */
