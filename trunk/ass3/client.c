#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdlib.h>

#include "shared.h"

typedef struct Server {
    char* port;
    FILE* to;
    FILE* from;
} Server;

typedef struct Match {
    int id;
    char* opponentName;
    char* port;
    Server server;
} Match;

typedef struct AgentInfo {
    char* name;
    int numMatches;
    int matchesRemaining;
    int port;
    Server server;
    Match* matches;
} AgentInfo;

typedef enum ClientError {
    SUCCESS,
    INCORRECT_ARG_COUNT,
    INVALID_NAME,
    INVALID_MATCH_COUNT,
    INVALID_PORT
} ClientError;

void exit_client(ClientError err) {
    switch (err) {
        case INCORRECT_ARG_COUNT:
            fprintf(stderr, "Usage: rpsclient name matches port\n");
            break;
        case INVALID_NAME:
            fprintf(stderr, "Invalid name\n");
            break;
        case INVALID_MATCH_COUNT:
            fprintf(stderr, "Invalid match count\n");
            break;
        case INVALID_PORT:
            fprintf(stderr, "Invalid port number\n");
            break;
        default:
            break;
    }

    exit(err);
}

Server init_server() {
    Server server;
    server.port = malloc(0);

    return server;
}

AgentInfo init_agent() {
    AgentInfo info;
    info.name = malloc(0);
    info.matches = malloc(0);
    info.numMatches = 0;
    info.port = 0;
    info.server = init_server();

    return info;
}

bool parse_name(char* name, AgentInfo* info) {
    for (int c = 0; c < strlen(name); c++) {
        if (!isalnum(name[c])) {
            return false;
        }
    }

    if (!strcmp(name, "TIE") || !strcmp(name, "ERROR")) {
        return false;
    }

    strcpy(info->name, name);
    return true;
}

bool parse_num_matches(char* matches, AgentInfo* info) {
    int numMatches = strtol(matches, NULL, 10);
    if (numMatches == 0) {
        return false;
    }

    info->numMatches = numMatches;
    info->matchesRemaining = numMatches;
    info->matches = realloc(info->matches, sizeof(Match) * numMatches);
    return true;
}

bool connect_to_server(Server* info, char* port) {
    // get the address info on localhost
    struct addrinfo* ai = 0;
    struct addrinfo hints;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use the localhost

    int err;
    if ((err = getaddrinfo(NULL, port, &hints, &ai)) != 0) {
        freeaddrinfo(ai);
        return false; // failed to getaddrinfo
    }

    // get the socket descriptor
    int sockfd;
    if ((sockfd = socket(ai->ai_family, ai->ai_socktype, 0)) == -1) {
        return false; // failed socket
    }

    // connect to the socket
    if (connect(sockfd, (struct sockaddr*)ai->ai_addr, ai->ai_addrlen) == -1) {
        close(sockfd);
        return false; // failed to connect to socket
    }

    // dup to get seperate streams that can be closed independently
    int fd = dup(sockfd);
    info->to = fdopen(sockfd, "w");
    info->from = fdopen(fd, "r");
    strcpy(info->port, port);
    return true;
}

bool listen_on_ephemeral(AgentInfo* info) {
    // get the address info on localhost
    struct addrinfo* ai = 0;
    struct addrinfo hints;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use the localhost

    if (getaddrinfo(NULL, "0", &hints, &ai)) {
        freeaddrinfo(ai);
        return false;
    }

    // get the socket descriptor
    int sockfd;
    if ((sockfd = socket(ai->ai_family, ai->ai_socktype, 0)) == -1) {
        return false;
    }

    // bind the socket to the port from the getaddrinfo
    if (bind(sockfd, (struct sockaddr*)ai->ai_addr, sizeof(struct sockaddr))) {
        return false;
    }

    struct sockaddr_in ad;
    memset(&ad, 0, sizeof(struct sockaddr_in));
    socklen_t len = sizeof(struct sockaddr_in);
    if (getsockname(sockfd, (struct sockaddr*)&ad, &len)) {
        return false;
    }
    info->port = ntohs(ad.sin_port);
    return true;
}

void send_match_request(AgentInfo* info) {
    fprintf(info->server.to, "MR:%s:%d\n", info->name, info->port);
    fflush(info->server.to);
}

ClientError read_match_message(FILE* from, Match* match) {
    char* line = read_line(from);

    if (!strcmp(line, "BADNAME")) {
        return INVALID_NAME;
    }

    // skip past the MATCH
    int location = strlen("MATCH:"); 

    // read the match id
    sscanf(line + location, "%d", &match->id);

    // we're going to read both of these character by character
    match->opponentName = malloc(0);
    match->port = malloc(0);
    int opponentNameLength = 0;
    int portLength = 0;

    int count = 0;
    while (line[location] != '\0') {
        if (line[location] == ':') {
            count++;
            location++;
            continue;
        }

        if (count == 0) {
            location++;
            continue;
        } else if (count == 1) {
            match->opponentName = realloc(match->opponentName, sizeof(char) *
                    ++opponentNameLength);
            match->opponentName[opponentNameLength - 1] = line[location++];
        } else if (count == 2) {
            match->port = realloc(match->port, sizeof(char) * ++portLength);
            match->port[portLength - 1] = line[location++];
        } else {
            continue;
        }
    }

    return SUCCESS;
}

void run_match(AgentInfo* info, Match* match) {
    connect_to_server(&match->server, match->port);

    // play
}

ClientError run_matchup_loop(AgentInfo* info) {
    int currentMatch;
    ClientError err;

    while (info->matchesRemaining > 0) {
        currentMatch = info->numMatches - info->matchesRemaining;
        send_match_request(info);
        if ((err = read_match_message(info->server.from, 
                &info->matches[currentMatch])) != SUCCESS) {
            return err;
        }

        run_match(info, &info->matches[currentMatch]);

        // send RESULT message

        info->matchesRemaining--;
    }

    return SUCCESS;
}

int main(int argc, char** argv) {
    if (argc != 4) {
        exit_client(INCORRECT_ARG_COUNT);
    }

    AgentInfo info = init_agent();
    if (!parse_name(argv[1], &info)) {
        exit_client(INVALID_NAME);
    }

    if (!parse_num_matches(argv[2], &info)) {
        exit_client(INVALID_MATCH_COUNT);
    }

    if (!connect_to_server(&info.server, argv[3])) {
        exit_client(INVALID_PORT);
    }
    
    if (!listen_on_ephemeral(&info)) {
        exit_client(INVALID_PORT); // not sure what else to do here
    }

    ClientError err;
    if ((err = run_matchup_loop(&info)) != SUCCESS) {
        exit_client(err);
    }

    // read the match message
    // run_game_loop()
    // usleep(50000)
    return 0;
}
