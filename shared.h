#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>

#ifndef SHARED_H
#define SHARED_H

#define INITIAL_BUFFER_SIZE 80

typedef enum GameResult {
    WIN,
    LOSE,
    TIE
} GameResult;

typedef struct Result {
    char* player;
    GameResult result;
} Result;

// A first in, first out (FIFO) queue. 
// This data structure (by itself) is not thread safe.
struct Queue {
    // An offset within that array that points to the beginning of the queue
    // (where new data should be written).
    int writeEnd;
    // An offset within that array that points to the end of the queue (where
    // old data should be read).
    int readEnd;
    // The data contained in the queue - as an array.
    void** data;
    // How large the elements are that we are storing
    // We need this to prevent ourselves from moving into bad memory
    size_t elementSize;
    // the size of the queue
    int size;
};

// A threadsafe channel. 
// Data can be written to the channel or read from the
// channel at different times, by different threads - safely.
struct Channel {
    struct Queue inner;
    pthread_mutex_t lock;
    sem_t guard;
};

typedef struct Player {
    char* name;
    int wins;
    int ties;
    int losses;
} Player;

void print_results(struct Channel* channel);

// Creates (and returns) a new, empty channel, with no data in it.
struct Channel new_channel(size_t elementSize);

// Destroys an old channel. Takes as arguments a pointer to the channel, as
// well as a function to use to clean up any elements left in the channel (for
// example free). If the function provided is NULL, no clean up will be
// performed on these elements.
void destroy_channel(struct Channel* channel, void (*clean)(void*));

// Attempts to write a piece of data to the channel. Takes as arguments a
// pointer to the channel, and the data being written. Returns true if the
// attempt was successful, and false if the channel was full and unable to be
// written to.
bool write_channel(struct Channel* channel, void* data);

// Attempts to read a piece of data from the channel. Takes as arguments a
// pointer to the channel, and a pointer to where the data should be stored on
// a successful channel. On success, returns true and sets *output to the read
// data. On failure (due to empty channel), returns false and does not touch
// *output.
bool read_channel(struct Channel* channel, void** output);

// Creates (and returns) a new, empty queue, with no data in it.
struct Queue new_queue(size_t elementSize);

// Destroys an old queue. Takes as arguments a pointer to the queue, as well as
// a function to use to clean up any elements left in the queue (for example
// free). If the function provided is NULL, no clean up will be performed on
// these elements.
void destroy_queue(struct Queue* queue, void (*clean)(void*));

// Attempts to write a piece of data to the queue. Takes as arguments a pointer
// to the queue, and the data being written. Returns true if the attempt was
// successful, and false if the queue was full and unable to be written to.
bool write_queue(struct Queue* queue, void* data);

// Attempts to read a piece of data from the queue. Takes as arguments a
// pointer to the queue, and a pointer to where the data should be stored on a
// successful queue. On success, returns true and sets *output to the read
// data. On failure (due to empty queue), returns false and does not touch
// *output.
bool read_queue(struct Queue* queue, void** output);

char* read_line(FILE*);
bool check_tag(char*, char*);

#endif
