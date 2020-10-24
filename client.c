#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdlib.h>

#include "shared.h"

#define SLEEP_TIME 50000
#define MAX_MATCHES 20

/**
 * Represents the information for a server
 *
 * port: the port of the server
 * to: file descriptor, the server is listening to this
 * from: file descriptor, the server will write here
 *
 */
typedef struct Server {
    char* port;
    FILE* to;
    FILE* from;
} Server;

/** The possible game results */
typedef enum Result {
    WIN,
    LOSE,
    TIE
} Result;

/**
 * Represents the information for a match from the perspective of an agent.
 *
 * id: the id of this match
 * opponentName: the name of the opponent in the match
 * port: the port to connect to, to play
 * server: the server information for the opponent
 *
 */
typedef struct Match {
    int id;
    char* opponentName;
    char* port;
    int opponentScore;
    int playerScore;
    Result result;
    Server server;
} Match;

/**
 * Represents an agent/client that can play a match.
 *
 * name: the name of this agent
 * numMatches: the number of matches this agent will play
 * matchesRemaining: the number of matches this agent has left to play
 * port: the port this agent is listening on
 * server: the rpsserver info
 * matches: the matches that this agent will play
 *
 */
typedef struct AgentInfo {
    char* name;
    int numMatches;
    int matchesRemaining;
    int port;
    int socketFd;
    Server server;
    Match* matches;
} AgentInfo;

/** The possible errors, as per the specification, includes an UNSPECIFIED
 * type for situations not being tested.
 */
typedef enum ClientError {
    SUCCESS,
    INCORRECT_ARG_COUNT,
    INVALID_NAME,
    INVALID_MATCH_COUNT,
    INVALID_PORT,
    UNSPECIFIED
} ClientError;

/** The possible moves */
typedef enum MoveType {
    ROCK,
    PAPER,
    SCISSORS
} MoveType;

/**
 * Exits the client with a given error.
 *
 * err (ClientError): the error to exit with
 *
 */
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

/**
 * Initialises a server struct.
 *
 * Returns a new server struct.
 *
 */
Server init_server() {
    Server server;
    server.port = malloc(0);

    return server;
}

/**
 * Initialises an agent struct
 *
 * Returns a new agent struct.
 *
 */
AgentInfo init_agent() {
    AgentInfo info;
    info.name = malloc(0);
    info.matches = malloc(0);
    info.numMatches = 0;
    info.port = 0;
    info.server = init_server();

    return info;
}

/**
 * Parse the name from the arguments
 *
 * name (char*): the name to parse
 * info (AgentInfo*): the agent to store this name in
 *
 * Returns SUCCESS if successful, otherwise the appropriate error.
 *
 */
ClientError parse_name(char* name, AgentInfo* info) {
    for (int c = 0; c < strlen(name); c++) {
        if (!isalnum(name[c])) {
            return INVALID_NAME;
        }
    }

    if (!strcmp(name, "TIE") || !strcmp(name, "ERROR")) {
        return INVALID_NAME;
    }

    strcpy(info->name, name);
    return SUCCESS;
}

/**
 * Parse the number of matches from the arguments
 *
 * matches (char*): the number to parse
 * info (AgentInfo*): the agent to store this number in
 *
 * Returns SUCCESS if successful, otherwise the appropriate error.
 *
 */
ClientError parse_num_matches(char* matches, AgentInfo* info) {
    int numMatches = strtol(matches, NULL, 10);
    if (numMatches == 0) {
        return INVALID_MATCH_COUNT;
    }

    info->numMatches = numMatches;
    info->matchesRemaining = numMatches;
    info->matches = realloc(info->matches, sizeof(Match) * numMatches);
    return SUCCESS;
}

/**
 * Connect to the server on localhost at the given port.
 *
 * info (Server*): store the information about this server here
 * port (char*): the port to connect to
 *
 * Returns SUCCESS if successful, otherwise the appropriate error.
 *
 */
ClientError connect_to_server(Server* info, char* port) {
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
        return INVALID_PORT; // failed to getaddrinfo
    }

    // get the socket descriptor
    int sockfd;
    if ((sockfd = socket(ai->ai_family, ai->ai_socktype, 0)) == -1) {
        return INVALID_PORT;
    }

    // connect to the socket
    if (connect(sockfd, (struct sockaddr*)ai->ai_addr, ai->ai_addrlen)) {
        close(sockfd);
        return INVALID_PORT;
    }


    // dup to get seperate streams that can be closed independently
    int fd = dup(sockfd);
    info->to = fdopen(sockfd, "w");
    info->from = fdopen(fd, "r");
    strcpy(info->port, port);
    return SUCCESS;
}

/**
 * Listen on an ephemeral port on localhost.
 *
 * port (int*): store the listened to port here
 *
 * Returns SUCCESS if successful, otherwise the appropriate error.
 *
 */
ClientError listen_on_ephemeral(int* port, int* socketFd) {
    // get the address info on localhost
    struct addrinfo* ai = 0;
    struct addrinfo hints;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET; // IPv4
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use the localhost

    if (getaddrinfo(NULL, "0", &hints, &ai)) {
        freeaddrinfo(ai);
        return INVALID_PORT;
    }

    // get the socket descriptor
    int sockfd;
    if ((sockfd = socket(ai->ai_family, ai->ai_socktype, 0)) == -1) {
        return INVALID_PORT;
    }
    *socketFd = sockfd;

    // bind the socket to the port from the getaddrinfo
    if (bind(sockfd, (struct sockaddr*)ai->ai_addr, sizeof(struct sockaddr))) {
        return INVALID_PORT;
    }

    struct sockaddr_in ad;
    memset(&ad, 0, sizeof(struct sockaddr_in));
    socklen_t len = sizeof(struct sockaddr_in);
    if (getsockname(sockfd, (struct sockaddr*)&ad, &len)) {
        return INVALID_PORT;
    }

    if (listen(sockfd, 10)) {
        //
    }

    *port = ntohs(ad.sin_port);
    return SUCCESS;
}

/**
 * Send a match request to the server.
 *
 * info (AgentInfo*): the info to be sent, also holds the file descriptor
 *
 */
void send_match_request(AgentInfo* info) {
    fprintf(info->server.to, "MR:%s:%d\n", info->name, info->port);
    fflush(info->server.to);
}

/**
 * Compares two moves according to the game rules.
 *
 * playerMove (MoveType): the first move
 * opponentMove (MoveType): the second move
 *
 * Returns the result from the perspective of the player
 *
 */
Result compare_moves(MoveType playerMove, MoveType opponentMove) {
    switch (playerMove) {
        case ROCK:
            if (opponentMove == PAPER) {
                return LOSE;
            } else if (opponentMove == SCISSORS) {
                return WIN;
            }
            break;
        case SCISSORS:
            if (opponentMove == ROCK) {
                return LOSE;
            } else if (opponentMove == PAPER) {
                return WIN;
            }
            break;
        case PAPER:
            if (opponentMove == SCISSORS) {
                return LOSE;
            } else if (opponentMove == ROCK) {
                return WIN;
            }
            break;
    }
    return TIE;
}

/**
 * Read a match message recieved from the server.
 *
 * from (FILE*): the file descriptor to read from
 * match (Match*): the match to be initialised
 *
 * Returns SUCCESS if successful, otherwise the appropriate error.
 *
 */
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

    match->server = init_server();
    return SUCCESS;
}

/**
 * Converts a MoveType enum to string format.
 *
 * type (MoveType): the move to convert
 *
 * The string representation of this move.
 *
 */
char* move_as_string(MoveType type) {
    char* moves[3] = {"ROCK", "PAPER", "SCISSORS"};
    return moves[type];
}

/**
 * Converts a Result enum to string format.
 *
 * type (Result): the result to convert
 *
 * The string representation of this result.
 *
 */
char* result_as_string(Result result) {
    char* results[3] = {"WIN", "LOSE", "TIE"};
    return results[result];
}

/**
 * Read a move message from the opponent.
 *
 * stream (FILE*): the stream of the opponent
 *
 * Returns the move made by the opponent.
 *
 */
MoveType read_move_message(FILE* stream) {
    char* line = read_line(stream);
    char* move = malloc(0);
    int length = 0;

    int location = strlen("MOVE:");
    while (line[location] != '\0') {
        move = realloc(move, sizeof(char) * ++length);
        move[length - 1] = line[location++];
    }

    if (!strcmp(move, "ROCK")) {
        return ROCK;
    } else if (!strcmp(move, "PAPER")) {
        return PAPER;
    } else {
        return SCISSORS;
    }
}

void print_match_results(Match* matches, int numMatches) {
    for (int i = 0; i < numMatches; i++) {
        Match current = matches[i];
        printf("%d %s %s\n", current.id, current.opponentName,
                result_as_string(current.result));
    }
}

/**
 * Play a single match with the opponent at the next port.
 * 
 * info (AgentInfo*): the info of the current agent
 * match (Match*): the info with the current match
 *
 * Returns SUCCESS if successful.
 *
 */
ClientError play_match(AgentInfo* info, Match* match) {
    ClientError err;
    if ((err = connect_to_server(&match->server, match->port)) != SUCCESS) {
        //
        return INVALID_PORT;
    }

    int opponentFd = accept(info->socketFd, 0, 0);
    FILE* opponent = fdopen(opponentFd, "r");

    MoveType move, opponentMove;
    for (int round = 0; round < MAX_MATCHES; round++) {
        // generate and send move
        move = (MoveType) (rand() % 3);
        fprintf(match->server.to, "MOVE:%s\n", move_as_string(move));
        fflush(match->server.to);

        // recieve the move from the opponent
        opponentMove = read_move_message(opponent);
        
        Result result = compare_moves(move, opponentMove);
        
        if (result == WIN) {
            match->playerScore++;
        } else if (result == LOSE) {
            match->opponentScore++;
        }

        if (round >= 4) {
            if (match->playerScore > match->opponentScore) {
                fprintf(info->server.to, "RESULT:%d:%s\n", match->id, 
                        info->name);
                fflush(info->server.to);
                match->result = WIN;
                return SUCCESS;
            } else if (match->playerScore < match->opponentScore) {
                fprintf(info->server.to, "RESULT:%d:%s\n", match->id, 
                        match->opponentName);
                fflush(info->server.to);
                match->result = LOSE;
                return SUCCESS;
            }
        }
    }

    fprintf(info->server.to, "RESULT:%d:TIE\n", match->id);
    fflush(info->server.to);
    match->result = TIE;
    return SUCCESS;
}

/**
 * Run the matchup loop for a client.
 *
 * info (AgentInfo*): the info for this agent.
 *
 * Returns SUCCESS if successful.
 */
ClientError run_matchup_loop(AgentInfo* info) {
    int currentMatch;
    ClientError err;

    while (info->matchesRemaining > 0) {
        if ((err = connect_to_server(&info->server, info->server.port)) 
                != SUCCESS) {
            return err;
        }

        currentMatch = info->numMatches - info->matchesRemaining;
        
        send_match_request(info);
        
        err = UNSPECIFIED;
        while (err != SUCCESS) {
            err = read_match_message(info->server.from,
                    &info->matches[currentMatch]);
        }

        if ((err = play_match(info, &info->matches[currentMatch])) 
                != SUCCESS) {
            return err;
        }

        info->matchesRemaining--;
        usleep(SLEEP_TIME);
    }
    print_match_results(info->matches, info->numMatches);

    return SUCCESS;
}

int main(int argc, char** argv) {
    if (argc != 4) {
        exit_client(INCORRECT_ARG_COUNT);
    }

    AgentInfo info = init_agent();
    ClientError err;
    if ((err = parse_name(argv[1], &info)) != SUCCESS) {
        exit_client(err);
    }

    // seed the random number generator according to the specified algorithm
    int seed = 0;
    for (int i = 0; i < strlen(info.name); i++) {
        seed += info.name[i];
    }
    srand(seed);

    if ((err = parse_num_matches(argv[2], &info)) != SUCCESS) {
        exit_client(err);
    }

    info.server.port = argv[3];
    /*
    if ((err = connect_to_server(&info.server, argv[3])) != SUCCESS) {
        exit_client(err);
    }
    */
    
    if ((err = listen_on_ephemeral(&info.port, &info.socketFd)) != SUCCESS) {
        exit_client(err); // not sure what else to do here
    }

    if ((err = run_matchup_loop(&info)) != SUCCESS) {
        exit_client(err);
    }

    return 0;
}
