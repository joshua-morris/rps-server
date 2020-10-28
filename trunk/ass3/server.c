#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>

#include "queue.h"
#include "channel.h"
#include "shared.h"

#define BACKLOG 128
#define MAX_INPUT 80

/** Exit codes defined on spec */
typedef enum ServerError {
    INCORRECT_ARG_COUNT
} ServerError;

/**
 * A player and it's results
 *
 * name (char*): the player name
 * wins (int): the number of wins
 * ties (int): the number of ties
 * losses (int): the number of losses
 *
 */
typedef struct Player {
    char* name;
    int wins;
    int ties;
    int loses;
} Player;

/**
 * A match request
 *
 * name (char*): the player name
 * port (char*): the port they are listening on
 *
 */
typedef struct Request {
    char* name;
    char* port;
} Request;

/**
 * Represents a client connected to the server
 *
 * stream (FILE*): the input stream
 * id (pthread_t): the thread we want a MR from
 * requests (struct Channel*): points to the requests queue
 *
 */
typedef struct Client {
    FILE* stream;
    pthread_t id;
    struct Channel* requests;
    Request request;
} Client;

/**
 * The information related to a server
 *
 * players (Player*): the players and their results (to be printed on SIGHUP)
 * requests (struct Channel): the match requests queued
 * results (struct Channel): the results queued
 * clients (Client*): the clients connected to this server
 * numClients (int): the number of clients connected
 * socketFd (int): the fd of this servers socket
 *
 */
typedef struct ServerInfo {
    Player* players;
    struct Channel requests;
    struct Channel results;
    Client** clients;
    int numClients;
    int socketFd;
} ServerInfo;

/**
 * Exit from the server
 *
 * err (ServerError): the error to exit with
 *
 */
void exit_server(ServerError err) {
    switch (err) {
        case INCORRECT_ARG_COUNT:
            fprintf(stderr, "Usage: rpsserver\n");
    }
    exit(err);
}

/**
 * Create and initialise a server with a struct
 *
 * info (ServerInfo*): the struct to initialise
 *
 * Returns 0 on success
 *
 */
int create_server(ServerInfo* info) {
    // get the address info on localhost on an ephemeral port
    struct addrinfo* ai = NULL;
    struct addrinfo hints;
    const char* port = "0";

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // on localhost

    int err;
    if ((err = getaddrinfo(NULL, port, &hints, &ai)) != 0) {
        freeaddrinfo(ai);
        fprintf(stderr, "%s\n", gai_strerror(err));
        return 1;
    }

    // get the socket descriptor
    int serv = socket(ai->ai_family, ai->ai_socktype, 0);
    info->socketFd = serv;
    info->requests = new_channel(sizeof(Request));
    info->numClients = 0;
    info->clients = malloc(info->numClients);

    // bind the socket to the port from the getaddrinfo
    if (bind(serv, (struct sockaddr*)ai->ai_addr, sizeof(struct sockaddr))) {
        freeaddrinfo(ai);
        perror("Binding");
        return 3;
    }
    freeaddrinfo(ai);

    struct sockaddr_in ad;
    memset(&ad, 0, sizeof(struct sockaddr_in));
    socklen_t len = sizeof(struct sockaddr_in);
    if (getsockname(serv, (struct sockaddr*)&ad, &len)) {
        perror("sockname");
        return 4;
    }
    printf("%u\n", ntohs(ad.sin_port));
    fflush(stdout);

    if (listen(serv, BACKLOG)) {
        return -1;
    }

    return 0;
}

/**
 * Read and parse a MR and keep the relevant data
 *
 * stream (FILE*): the stream to read from
 * client (Client*): the client to store data in
 *
 * Returns true if the message was valid
 *
 */
bool read_match_message(FILE* stream, Client* client) {
    char* line = read_line(stream);
    int location = 0;
    int count = 0;

    if (!check_tag("MR:", line)) {
        return false;
    }
    location += strlen("MR:");

    char* name = malloc(0);
    int nameLength = 0;

    char* port = malloc(0);
    int portLength = 0;
    while (line[location] != '\0') {
        if (line[location] == ':') {
            location++;
            count++;
            continue;
        }

        if (count == 0) {
            name = realloc(name, sizeof(char) * ++nameLength);
            name[nameLength - 1] = line[location++];
        } else if (count == 1) {
            port = realloc(port, sizeof(char) * ++portLength);
            port[portLength - 1] = line[location++];
        } else {
            return false;
        }
    }
    free(line);

    name = realloc(name, nameLength + 1);
    name[nameLength] = '\0';
    port = realloc(port, portLength + 1);
    port[portLength] = '\0';
 
    client->request.name = strdup(name);
    client->request.port = strdup(port);
    free(name);
    free(port);
    return true;
}

/**
 * Wait for a match request from a client
 *
 * clientArg (void*): the client, contains the FILE* stream and other
 * relavent information for the client
 *
 * Returns NULL
 *
 */
void* wait_for_request(void* clientArg) {
    Client* client = (Client*) clientArg;
    if (!(read_match_message(client->stream, client))) {
        fclose(client->stream);
    } else {
        Request* current = malloc(sizeof(Request));
        current->name = strdup(client->request.name);
        current->port = strdup(client->request.port);
        write_channel(client->requests, (void*) current);
    }
    return NULL;
}

/**
 * Read from the channel and pair up clients as appropriate (on a new thread)
 *
 * args (void*): will be cast to a Channel*
 *
 * Returns NULL
 */
void* match_clients(void* args) {
    struct Channel* requests = (struct Channel*) args;

    Request requestOne, requestTwo;
    while (1) {
        while (!(read_channel(requests, (void**) &requestOne))) {
            continue;
        }
        while (!(read_channel(requests, (void**) &requestTwo))) {
            continue;
        }
        printf("Player 1: %s, Port: %s\n Player 2: %s, Port: %s\n",
                requestOne.name, requestOne.port, requestTwo.name, 
                requestTwo.port);
    }

    return NULL;
}

/**
 * Begin accepting connections that a listen()ed to in the main thread.
 * This function will create a new thread for every accept()ed client
 * and listen for a MR.
 *
 * info (ServerInfo*): the info of this server
 *
 */
void take_connections(ServerInfo* info) {
    // we'll continue to loop and create threads as we pair up players
    // note that we never need to worry about terminating since that will
    // be handled by the SIGHUP
    FILE* current;
    while (true) {
        int clientFd = accept(info->socketFd, 0, 0);
        info->numClients++;
        info->clients = realloc(info->clients, 
                sizeof(Client*) * info->numClients);
        current = fdopen(clientFd, "r");
        info->clients[info->numClients - 1] = malloc(sizeof(Client*));
        info->clients[info->numClients - 1]->stream = current;
        info->clients[info->numClients - 1]->requests = &info->requests;
        pthread_create(&info->clients[info->numClients - 1]->id, NULL, 
                wait_for_request, 
                (void*) info->clients[info->numClients - 1]);
    }
}

int main(int argc, char** argv) {
    if (argc != 1) {
        exit_server(INCORRECT_ARG_COUNT);
    }

    ServerInfo info;

    int err;
    if ((err = create_server(&info)) != 0) {
        return err;
    }

    pthread_t id;
    pthread_create(&id, NULL, match_clients, (void*) &info.requests);
    take_connections(&info);

    return 0;
}
