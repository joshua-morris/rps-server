#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>

#include "shared.h"

#define BACKLOG 128
#define MAX_INPUT 80

// A global results struct used when handling signal hangup
struct Channel* globalResults;

/** Exit codes defined on spec */
typedef enum ServerError {
    INCORRECT_ARG_COUNT = 1
} ServerError;

/**
 * A match request
 *
 * name (char*): the player name
 * port (char*): the port they are listening on
 * stream (FILE*): so we can send a message to this client
 *
 */
typedef struct Request {
    char* name;
    char* port;
    FILE* stream;
    struct Channel* results;
} Request;

typedef struct Match {
    char* playerPort;
    char* opponentPort;
    int id;
    char* opponentName;
    char* playerName;
    FILE* stream;
    struct Channel* results;
} Match;

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
    struct Channel* requests;
    pthread_t id;
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
    info->results = new_channel(sizeof(Result)); 
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
        current->stream = client->stream;
        current->results = client->request.results;
        write_channel(client->requests, (void*) current);
    }
    return NULL;
}

/**
 * Add a result to the results queue
 *
 * results (struct Channel*): the results queue
 * player (char*): the player to add
 * results (GameResult): the result to add
 *
 */
void add_result(struct Channel* results, char* player, GameResult result) {
    Result* newResult = malloc(sizeof(Result));
    newResult->player = player;
    newResult->result = result;
    write_channel(results, (void*) newResult);
}

/**
 * Read a RESULT message from the client
 *
 * stream (FILE*): the input stream
 * results (struct Channel*): the results queue
 * player (char*): the player
 *
 */
void read_result_message(FILE* stream, struct Channel* results, char* player) {
    char* line = read_line(stream);
    int count = 0;
    int location = sizeof("RESULT:");

    int resultLength = 0;
    char* result = malloc(0);
    while (line[location] != '\0') {
        if (line[location] == ':') {
            count++;
            location++;
            continue;
        }

        if (count == 1) {
            result = realloc(result, ++resultLength * sizeof(char));
            result[resultLength - 1] = line[location];
        }
        location++;
    }
    result[resultLength] = '\0';
    if (!strcmp("TIE", result)) {
        add_result(results, player, TIE);
    } else if (!strcmp(player, result)) {
        add_result(results, player, WIN);
    } else {
        add_result(results, player, LOSE);
    }

    free(result);
    free(line);
}

/**
 * Start a new match on a new thread. From the perspective
 * of a single agent, waits for a RESULT.
 *
 * matchArg (void*): the match
 *
 * Returns NULL
 *
 */
void* new_match(void* matchArg) {
    Match* match = (Match*) matchArg;
    
    fprintf(match->stream, "MATCH:%d:%s:%s\n", match->id, match->opponentName,
            match->opponentPort);
    fflush(match->stream);
    read_result_message(match->stream, match->results, match->playerName);
    fclose(match->stream);
    return NULL;
}

/**
 * Read from the channel and pair up clients as appropriate (on a new thread)
 *
 * args (void*): will be cast to a Channel*
 *
 * Returns NULL
 *
 */
void* match_clients(void* args) {
    struct Channel* requests = (struct Channel*) args;

    Request requestOne, requestTwo;
    int match = 1;
    while (1) {
        while (1) {
            if (read_channel(requests, (void**) &requestOne)) {
                break;
            }
        }
        while (1) {
            if (read_channel(requests, (void**) &requestTwo)) {
                break;
            }
        }
        
        Match matchOne = {.playerPort = requestOne.port, 
                .opponentPort = requestTwo.port, 
                .playerName = requestOne.name,
                .opponentName = requestTwo.name, .id = match,
                .stream = requestOne.stream, 
                .results = requestOne.results}; 
        Match matchTwo = {.playerPort = requestTwo.port,
                .opponentPort = requestOne.port,
                .playerName = requestTwo.name,
                .opponentName = requestOne.name, .id = match,
                .stream = requestTwo.stream,
                .results = requestTwo.results};

        pthread_t playerOne, playerTwo;
        pthread_create(&playerOne, NULL, new_match, (void*) &matchOne);
        pthread_create(&playerTwo, NULL, new_match, (void*) &matchTwo);
        match++;
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
        current = fdopen(clientFd, "w+");
        info->clients[info->numClients - 1] = malloc(sizeof(Client));
        info->clients[info->numClients - 1]->stream = current;
        info->clients[info->numClients - 1]->requests = &info->requests;
        info->clients[info->numClients - 1]->request.results = &info->results;
        pthread_create(&info->clients[info->numClients - 1]->id, NULL, 
                wait_for_request, 
                (void*) info->clients[info->numClients - 1]);
    }
}

/**
 * Handles a signal hangup, prints out the game results
 *
 */
void handle_sighup() {
    print_results(globalResults);
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
    
    globalResults = &info.results;
   
    struct sigaction sa;
    sa.sa_handler = handle_sighup;
    sigaction(SIGHUP, &sa, 0);

    pthread_t id;
    pthread_create(&id, NULL, match_clients, (void*) &info.requests);
    take_connections(&info);

    return 0;
}
