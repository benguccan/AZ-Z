#include "queue.h"

void queue_init(SharedQueue* q) {
    if (q == NULL) {
        return;
    }

    q->front = 0;
    q->rear = 0;
    q->count = 0;

    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
}

int queue_push(SharedQueue* q, FishSample sample) {
    if (q == NULL) {
        return -1;
    }

    pthread_mutex_lock(&q->mutex);

    while (q->count == QUEUE_SIZE) {
        pthread_cond_wait(&q->not_full, &q->mutex);
    }

    q->buffer[q->rear] = sample;
    q->rear = (q->rear + 1) % QUEUE_SIZE;
    q->count++;

    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mutex);

    return 0;
}

int queue_pop(SharedQueue* q, FishSample* sample) {
    if (q == NULL || sample == NULL) {
        return -1;
    }

    pthread_mutex_lock(&q->mutex);

    while (q->count == 0) {
        pthread_cond_wait(&q->not_empty, &q->mutex);
    }

    *sample = q->buffer[q->front];
    q->front = (q->front + 1) % QUEUE_SIZE;
    q->count--;

    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->mutex);

    return 0;
}
