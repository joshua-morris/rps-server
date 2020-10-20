#include "channel.h"
#include "queue.h"

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
