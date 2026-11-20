#include "include/queue.h"
#include <string.h>

void queue_init(Queue *queue, void *elements, size_t element_size) {
    queue->elements = elements;
    queue->element_size = element_size;
    queue->head = 0;
    queue->size = 0;
    pthread_mutex_init(&queue->mutex, NULL);
}

void queue_destroy(Queue *queue) { pthread_mutex_destroy(&queue->mutex); }

bool queue_is_empty(Queue *queue) {
    pthread_mutex_lock(&queue->mutex);
    bool empty = queue->size == 0;
    pthread_mutex_unlock(&queue->mutex);
    return empty;
}

bool queue_enqueue(Queue *queue, const void *element) {
    pthread_mutex_lock(&queue->mutex);

    if (queue->size == QUEUE_CAPACITY) {
        pthread_mutex_unlock(&queue->mutex);
        return false;
    }

    size_t tail = (queue->head + queue->size) % QUEUE_CAPACITY;

    unsigned char *offset = queue->elements;
    memcpy(offset + (tail * queue->element_size), element, queue->element_size);

    queue->size++;

    pthread_mutex_unlock(&queue->mutex);
    return true;
}

bool queue_dequeue(Queue *queue, void *element) {
    pthread_mutex_lock(&queue->mutex);

    if (queue->size == 0) {
        pthread_mutex_unlock(&queue->mutex);
        return false;
    }

    unsigned char *offset = queue->elements;
    memcpy(element, offset + (queue->head * queue->element_size), queue->element_size);

    queue->head = (queue->head + 1) % QUEUE_CAPACITY;
    queue->size--;

    pthread_mutex_unlock(&queue->mutex);
    return true;
}
