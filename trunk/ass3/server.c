#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>

typedef enum ServerError {
    INCORRECT_ARG_COUNT
} ServerError;

void exit_server(ServerError err) {
    switch (err) {
        case INCORRECT_ARG_COUNT:
            fprintf(stderr, "Usage: rpsserver\n");
    }
}

int create_server() {
    struct addrinfo* ai = NULL;
    struct addrinfo hints;
    const char* port = "0"; // ephemeral port

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int err;
    if ((err = getaddrinfo("localhost", port, &hints, &ai)) != 0) {
        freeaddrinfo(ai);
        fprintf(stderr, "%s\n", gai_strerror(err));
        return 1;
    }

    int serv = socket(AF_INET, SOCK_STREAM, 0);
    if (bind(serv, (struct sockaddr*)ai->ai_addr, sizeof(struct sockaddr))) {
        perror("Binding");
        return 3;
    }

    struct sockaddr_in ad;
    memset(&ad, 0, sizeof(struct sockaddr_in));
    socklen_t len = sizeof(struct sockaddr_in);
    if (getsockname(serv, (struct sockaddr*)&ad, &len)) {
        perror("sockname");
        return 4;
    }
    printf("%u\n", ntohs(ad.sin_port));
    fflush(stdout);

    return 0;
}

int main(int argc, char** argv) {
    if (argc != 1) {
        exit_server(INCORRECT_ARG_COUNT);
    }

    int err;
    if ((err = create_server()) != 0) {
        return err;
    }

    return 0;
}
