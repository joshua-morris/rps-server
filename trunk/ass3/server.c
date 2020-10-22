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

    // bind the socket to the port from the getaddrinfo
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

    if (listen(serv, 10)) {

    }

    int conn_fd;
    char* msg = "hi";
    while ((conn_fd = accept(serv, 0, 0), conn_fd >= 0)) {
        FILE* stream = fdopen(conn_fd, "w");
        fputs(msg, stream);
        fflush(stream);
        fclose(stream);
    }

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
