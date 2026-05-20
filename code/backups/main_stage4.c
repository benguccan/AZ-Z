#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include "fish_system.h"
#include "queue.h"

#define SAMPLE_COUNT 5
#define NS_PER_MS 1000000ULL
#define DEADLINE_MS 3.0
#define DEADLINE_NS (3ULL * NS_PER_MS)
#define SENSOR_DELAY_US 400
#define CLASSIFICATION_DELAY_US 900
#define ACTUATOR_DELAY_US 700

static SharedQueue g_input_queue;
static SharedQueue g_output_queue;

static uint64_t get_timestamp_ns(void) {
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t) ts.tv_sec * 1000000000ULL + (uint64_t) ts.tv_nsec;
}

static const char* deadline_status_to_string(uint64_t latency_ns) {
    if (latency_ns > DEADLINE_NS) {
        return "DEADLINE_MISS";
    }

    return "DEADLINE_OK";
}

static const char* route_to_bin(FishClass class_id) {
    switch (class_id) {
        case FISH_SMALL:
            return "BIN_A";
        case FISH_MEDIUM:
            return "BIN_B";
        case FISH_LARGE:
            return "BIN_C";
        case FISH_INVALID:
            return "REJECT_BIN";
        default:
            return "UNKNOWN_BIN";
    }
}

static void* sensor_thread(void* arg) {
    FishSample samples[SAMPLE_COUNT] = {
        {1, 180, 220, 0, FISH_INVALID, ERR_NONE},
        {2, 280, 520, 0, FISH_INVALID, ERR_NONE},
        {3, 420, 850, 0, FISH_INVALID, ERR_NONE},
        {4, 0, 500, 0, FISH_INVALID, ERR_NONE},
        {5, 340, 680, 0, FISH_INVALID, ERR_NONE}
    };
    int i;

    (void) arg;

    for (i = 0; i < SAMPLE_COUNT; i++) {
        samples[i].timestamp_ns = get_timestamp_ns();
        samples[i].error_code = validate_fish_data(samples[i].length_mm, samples[i].weight_g)
            ? ERR_NONE
            : ERR_INVALID_DATA;

        printf(
            "[Sensor Thread] produced fish_id=%u length=%umm weight=%ug\n",
            samples[i].fish_id,
            samples[i].length_mm,
            samples[i].weight_g
        );

        queue_push(&g_input_queue, samples[i]);
        usleep(SENSOR_DELAY_US);
    }

    printf("[Sensor Thread] production completed\n");
    return NULL;
}

static void* classification_thread(void* arg) {
    int i;

    (void) arg;

    for (i = 0; i < SAMPLE_COUNT; i++) {
        FishSample sample;

        queue_pop(&g_input_queue, &sample);

        sample.class_id = classify_fish(sample.length_mm, sample.weight_g);
        if (sample.class_id == FISH_INVALID) {
            sample.error_code = ERR_INVALID_DATA;
        } else {
            sample.error_code = ERR_NONE;
        }

        printf(
            "[Classification Thread] consumed/classified fish_id=%u class=%s error=%s timestamp_ns=%llu\n",
            sample.fish_id,
            fish_class_to_string(sample.class_id),
            error_code_to_string(sample.error_code),
            (unsigned long long) sample.timestamp_ns
        );

        queue_push(&g_output_queue, sample);
        usleep(CLASSIFICATION_DELAY_US);
    }

    printf("[Classification Thread] classification completed\n");
    return NULL;
}

static void* actuator_thread(void* arg) {
    int i;

    (void) arg;

    for (i = 0; i < SAMPLE_COUNT; i++) {
        FishSample sample;
        uint64_t finish_timestamp_ns;
        uint64_t latency_ns;
        double latency_ms;

        queue_pop(&g_output_queue, &sample);
        finish_timestamp_ns = get_timestamp_ns();
        latency_ns = finish_timestamp_ns - sample.timestamp_ns;
        latency_ms = (double) latency_ns / (double) NS_PER_MS;

        if (latency_ns > DEADLINE_NS) {
            sample.error_code = ERR_DEADLINE_MISS;
        }

        printf(
            "[Actuator Thread] routed fish_id=%u class=%s destination=%s latency_ns=%llu latency_ms=%.3f status=%s error=%s\n",
            sample.fish_id,
            fish_class_to_string(sample.class_id),
            route_to_bin(sample.class_id),
            (unsigned long long) latency_ns,
            latency_ms,
            deadline_status_to_string(latency_ns),
            error_code_to_string(sample.error_code)
        );

        usleep(ACTUATOR_DELAY_US);
    }

    printf("[Actuator Thread] routing completed\n");
    return NULL;
}

int main(void) {
    pthread_t sensor_tid;
    pthread_t classifier_tid;
    pthread_t actuator_tid;

    queue_init(&g_input_queue);
    queue_init(&g_output_queue);

    printf("[Main] starting three-stage pipeline simulation\n");

    pthread_create(&sensor_tid, NULL, sensor_thread, NULL);
    pthread_create(&classifier_tid, NULL, classification_thread, NULL);
    pthread_create(&actuator_tid, NULL, actuator_thread, NULL);

    pthread_join(sensor_tid, NULL);
    pthread_join(classifier_tid, NULL);
    pthread_join(actuator_tid, NULL);

    printf("[Main] simulation completed\n");
    return 0;
}
