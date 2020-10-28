#include "queue.h"
#include <stdlib.h>
#include <string.h>

// Starting length of the queue.
const int QUEUE_CAPACITY = 10;

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
