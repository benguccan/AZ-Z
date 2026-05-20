#include <stdio.h>
#include <pthread.h>

void* worker(void* arg) {
    printf("pthread calisiyor\n");
    return NULL;
}

int main() {
    pthread_t t;
    if (pthread_create(&t, NULL, worker, NULL) != 0) {
        printf("pthread create hatasi\n");
        return 1;
    }
    pthread_join(t, NULL);
    return 0;
}
