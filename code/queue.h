#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>

#include "fish_system.h"

#define QUEUE_SIZE 3

typedef struct {
    FishSample sample;
} QueueNode;

typedef struct {
    FishSample buffer[QUEUE_SIZE];
    int front;
    int rear;
    int count;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} SharedQueue;

void queue_init(SharedQueue* q);
int queue_push(SharedQueue* q, FishSample sample);
int queue_pop(SharedQueue* q, FishSample* sample);

#endif
