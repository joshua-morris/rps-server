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

channel.o: channel.c channel.h shared.o
	$(CC) $(CFLAGS) shared.o -c channel.c -o channel.o

queue.o: queue.c queue.h
	$(CC) $(CFLAGS) -c queue.c -o queue.o	

rpsserver: server.c queue.o channel.o shared.o
	$(CC) $(CFLAGS) queue.o channel.o shared.o server.c -o rpsserver

rpsclient: client.c shared.o
	$(CC) $(CFLAGS) shared.o client.c -o rpsclient

clean:
	rm -f $(TARGETS) *.o
