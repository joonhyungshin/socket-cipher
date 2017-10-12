CC = gcc

all: server server_select client

cs341proj.o: cs341proj.c cs341proj.h
	$(CC) -c $<

libcs341proj.a: cs341proj.o
	ar rc $@ $<
	ranlib $@

CFLAGS = -Wall -I./ -L./ -lcs341proj

server: server.c libcs341proj.a server_common.c
	$(CC) $< -o $@ $(CFLAGS)

server_select: server_select.c libcs341proj.a server_common.c
	$(CC) $< -o $@ $(CFLAGS)

client: client.c libcs341proj.a
	$(CC) $< -o $@ $(CFLAGS)

clean:
	rm -f server server_select client cs341proj.o libcs341proj.a

.PHONY: all clean
