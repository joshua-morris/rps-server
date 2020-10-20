CC=gcc
CFLAGS=-Wall -pedantic -pthread -std=gnu99
TARGETS=rpsserver rpsclient
DEBUG= -g

.PHONY: all clean debug
.DEFAULT_GOAL: all

all: $(TARGETS)

debug: CFLAGS += $(DEBUG)
debug: $(TARGETS)

rpsserver: server.c
	$(CC) $(CFLAGS) server.c -o rpsserver

rpsclient: client.c
	$(CC) $(CFLAGS) client.c -o rpsclient

clean:
	rm -f $(TARGETS) *.o
