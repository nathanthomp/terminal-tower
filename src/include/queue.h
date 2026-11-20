#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>

#define QUEUE_CAPACITY 64

typedef struct {
    void *elements;
    size_t element_size;

    size_t head;
    size_t size;

    pthread_mutex_t mutex;
} Queue;

void queue_init(Queue *queue, void *elements, size_t element_size);
void queue_destroy(Queue *queue);
bool queue_is_empty(Queue *queue);
bool queue_enqueue(Queue *queue, const void *element);
bool queue_dequeue(Queue *queue, void *element);

#endif