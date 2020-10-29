#include "channel.h"
#include "queue.h"
#include "shared.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

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
