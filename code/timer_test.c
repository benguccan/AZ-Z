#include <stdio.h>
#include <time.h>

int main() {
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (volatile int i = 0; i < 1000000; i++);

    clock_gettime(CLOCK_MONOTONIC, &end);

    long sec = end.tv_sec - start.tv_sec;
    long nsec = end.tv_nsec - start.tv_nsec;
    long total_ns = sec * 1000000000L + nsec;

    printf("gecen sure (ns): %ld\n", total_ns);
    return 0;
}
