#include "channel.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Maximum length of string we are willing to accept
// (not production ready code, but good enough for a teaching example).
#define MAX_INPUT 80

// Wait 50ms between each attempt to read from the file, just to be sure
#define READ_FILE_DELAY_US 50000

void* reader_thread(void* args);
void* timer_thread(void* args);
void* writer_thread(void* args);

int main(int argc, char** argv) {
    struct Channel channel = new_channel(sizeof(char*));

    pthread_t threads[4];
    pthread_create(threads, NULL, reader_thread, (void*) &channel);
    pthread_create(threads + 1, NULL, timer_thread, (void*) &channel);
    pthread_create(threads + 2, NULL, writer_thread, (void*) &channel);
    //pthread_create(threads + 3, NULL, writer_thread, (void*) &channel);

    // wait until all threads are finished (these threads all run forever but
    // its best practice)
    for (int i = 0; i < 4; ++i) {
        pthread_join(threads[i], NULL);
    }

    destroy_channel(&channel, free);

    return 0;
}

// Thread handler function for the reader thread. 
// 
// This thread will read lines from stdin, and write them to the
// channel for further processing. Takes as an argument a pointer to
// the channel to write to. Returns NULL.
void* reader_thread(void* args) {
    
    struct Channel* channel = (struct Channel*) args;
    char buffer[MAX_INPUT];
    char* string;

    // break on EOF
    while ((string = fgets(buffer, MAX_INPUT, stdin))) {

        if (string[strlen(string) - 1] == '\n') {
            string[strlen(string) - 1] = '\0';
        }
        write_channel(channel, (void*) strdup(string));
    }

    return NULL;
}

// Thread handler for the timer thead. 
//
// This thread will write a line (from a file stuff.txt) to a
// channel periodically. When it reaches the end of the file it
// will loop back around. Takes as an argument a pointer to the
// channel to write to. Returns NULL if the file cannot be
// found, otherwise runs forever and does not return.
void* timer_thread(void* args) {
    
    struct Channel* channel = (struct Channel*) args;
    char buffer[MAX_INPUT];
    char* string;

    FILE* input = fopen("stuff.txt", "r");
    if (!input) {
        return NULL;
    }

    while (1) {
        while ((string = fgets(buffer, MAX_INPUT, input))) {
            if (string[strlen(string) - 1] == '\n') {
                string[strlen(string) - 1] = '\0';
            }
            write_channel(channel, (void*) strdup(string));
            usleep(READ_FILE_DELAY_US);
        }
        rewind(input);
    }

    // cleanup (will never happen as the thread runs forever)
    fclose(input);
    return NULL;
}

// Thread handler for the writer thread. 
//
// This thread will read lines from a channel, and write them to
// stdout. Takes as arguments a pointer to the channel. Does not
// return, as the function runs forever.
void* writer_thread(void* args) {
    
    struct Channel* channel = (struct Channel*) args;
    char* string;

    while (1) {
        if (read_channel(channel, (void**) &string)) {
            printf("%s\n", string);
            free(string);
        }
    }
    // will never happen as the loop runs forever
    return NULL;
}
