#include "shared.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/**
 * Reads a line of input from the given input stream.
 *
 * stream (FILE*): the file to read from
 *
 * Returns the line of input read. If EOF is read, returns NULL instead.
 *
 */
char* read_line(FILE* stream) {
    int bufferSize = INITIAL_BUFFER_SIZE;
    char* buffer = malloc(sizeof(char) * bufferSize);
    int numRead = 0;
    int next;

    while (1) {
        next = fgetc(stream);
        if (next == EOF && numRead == 0) {
            free(buffer);
            return NULL;
        }
        if (numRead == bufferSize - 1) {
            bufferSize *= 2;
            buffer = realloc(buffer, sizeof(char) * bufferSize);
        }
        if (next == '\n' || next == EOF) {
            buffer[numRead] = '\0';
            break;
        }
        buffer[numRead++] = next;
    }
    return buffer;
}

/**
 * Compare the tag with a line to check if it is of that type.
 *
 * tag (char*): the tag to check
 * line (char*): the line to check
 *
 * Returns true if the line begins with tag.
 *
 */
bool check_tag(char* tag, char* line) {
    return strncmp(tag, line, strlen(tag)) == 0;
}


// Starting length of the queue.
const int QUEUE_CAPACITY = 1000;

struct Queue new_queue(size_t elementSize) {

    struct Queue output;

    output.data = malloc(elementSize * QUEUE_CAPACITY);
    output.readEnd = -1; // queue is empty
    output.writeEnd = 0; // put first piece of data at the start of the queue
    output.elementSize = elementSize;

    return output;
}

void destroy_queue(struct Queue* queue, void (*clean)(void*)) {
    
    void* data;
    while (read_queue(queue, &data)) {
        clean(data);
    }
    free(queue->data);
}

bool write_queue(struct Queue* queue, void* data) {
    
    if (queue->writeEnd == queue->readEnd) {
        return false;   // queue is full
    }

    // Make a new block of memory to store our data in
    // Then copy the new item into it
    void* newElement = malloc(queue->elementSize);
    memcpy(newElement, data, queue->elementSize);

    // Now we put the new element into the queue
    queue->data[queue->writeEnd] = newElement;

    // if queue was empty, signal that the queue is no longer empty
    if (queue->readEnd == -1) {
        queue->readEnd = queue->writeEnd;
    }
    queue->writeEnd = (queue->writeEnd + 1) % QUEUE_CAPACITY;

    return true;
}

bool read_queue(struct Queue* queue, void** output) {
    
    if (queue->readEnd == -1) {
        // queue is empty
        return false;
    }
    
    // Get the stored element from the queue
    void* retrievedElement = queue->data[queue->readEnd];

    // Update the pointer to point to the stored memory
    memcpy(output, retrievedElement, queue->elementSize);

    // Free the block of memory we allocated for the element
    free(queue->data[queue->readEnd]);

    queue->readEnd = (queue->readEnd + 1) % QUEUE_CAPACITY;

    if (queue->readEnd == queue->writeEnd) {
        queue->readEnd = -1;
    }

    return true;
}

struct Channel new_channel(size_t elementSize) {

    struct Channel output;
    output.inner = new_queue(elementSize);
    pthread_mutex_init(&output.lock, NULL);
    sem_init(&output.guard, 0, 1);
    return output;
}   

void destroy_channel(struct Channel* channel, void (*clean)(void*)) {
    destroy_queue(&channel->inner, clean);
}

bool write_channel(struct Channel* channel, void* data) {
    pthread_mutex_lock(&channel->lock);
    bool output = write_queue(&channel->inner, data);
    pthread_mutex_unlock(&channel->lock);

    sem_post(&channel->guard);
    return output;
}

bool read_channel(struct Channel* channel, void** out) {
    sem_wait(&channel->guard);

    pthread_mutex_lock(&channel->lock);
    bool output = read_queue(&channel->inner, out);
    pthread_mutex_unlock(&channel->lock);
    return output;
}

bool contains_player(Player* players, char* player, int numPlayers) {
    for (int i = 0; i < numPlayers; i++) {
        Player current = players[i];
        if (!(strcmp(current.name, player))) {
            return true;
        }
    }
    return false;
}

void increase_result(Player** players, char* player, int result,
        int numPlayers) {
    int index = -1;
    for (int i = 0; i < numPlayers; i++) {
        if (!(strcmp((*players)[i].name, player))) {
            index = i;
            break;
        }
    }

    if (result == TIE) {
        (*players)[index].ties++;
    } else if (result == WIN) {
        (*players)[index].wins++;
    } else {
        (*players)[index].losses++;
    }
}

void print_results(struct Channel* channel) {
    int numPlayers = 0;
    Player* results = malloc(0);
    Result* current;

    sem_wait(&channel->guard);

    pthread_mutex_lock(&channel->lock);
    
    for (int i = 0; i < channel->inner.writeEnd; i++) {
        current = channel->inner.data[i];
        if (!(contains_player(results, current->player, numPlayers))) {
            numPlayers++;
            results = realloc(results, sizeof(Player) * numPlayers);
            Player newPlayer = {.name = current->player, .wins = 0, .ties = 0,
                .losses = 0};
            results[numPlayers - 1] = newPlayer;
        }
        increase_result(&results, current->player, current->result,
                numPlayers);
    }
    
    for (int i = 0; i < numPlayers - 1; i++) {
        for (int j = 0; j < numPlayers - 1 - i; j++) {
            if (strcmp(results[j].name, results[j + 1].name) > 0) {
                Player temp = results[j];
                results[j] = results[j + 1];
                results[j + 1] = temp;
            }
        }
    }

    for (int i = 0; i < numPlayers; i++) {
        printf("%s %d %d %d\n", results[i].name, results[i].wins,
                results[i].losses, results[i].ties);
        fflush(stdout);
    }
    printf("---\n");
    fflush(stdout);
    pthread_mutex_unlock(&channel->lock);
}

