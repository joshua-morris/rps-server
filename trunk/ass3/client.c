#include <stdio.h>

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
}

int main(int argc, char** argv) {
    if (argc != 4) {
        exit_client(INCORRECT_ARG_COUNT);
    }
}
