CC=gcc
CFLAGS=-Wall -pedantic -pthread -std=gnu99
TARGETS=rpsserver rpsclient
DEBUG= -g

.PHONY: all clean debug
.DEFAULT_GOAL: all

all: $(TARGETS)

debug: CFLAGS += $(DEBUG)
debug: $(TARGETS)

shared.o: shared.c shared.h
	$(CC) $(CFLAGS) -c shared.c -o shared.o

rpsserver: server.c shared.o
	$(CC) $(CFLAGS) shared.o server.c -o rpsserver

rpsclient: client.c shared.o
	$(CC) $(CFLAGS) shared.o client.c -o rpsclient

clean:
	rm -f $(TARGETS) *.o
