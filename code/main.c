#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "fish_system.h"
#include "logger.h"
#include "queue.h"

#define DEMO_MODE 1
#define STRESS_MODE 0

#if DEMO_MODE == STRESS_MODE
#error "Exactly one of DEMO_MODE or STRESS_MODE must be enabled."
#endif

#if DEMO_MODE
#define SAMPLE_COUNT 5
#else
#define SAMPLE_COUNT 120
#endif

#define NS_PER_MS 1000000ULL
#define DEADLINE_MS 3.0
#define DEADLINE_NS (3ULL * NS_PER_MS)
#define SENSOR_DELAY_US 400
#define CLASSIFICATION_DELAY_US 900
#define ACTUATOR_DELAY_US 700
#define DEMO_DEADLINE_MISS_FISH_ID 5U
#define TEST_DEADLINE_MISS_DELAY_US 4000

typedef struct {
    int total_fish;
    int deadline_ok_count;
    int deadline_miss_count;
    int invalid_data_count;
    double total_latency_ms;
    double max_latency_ms;
    pthread_mutex_t mutex;
} PerformanceStats;

static SharedQueue g_input_queue;
static SharedQueue g_output_queue;
static PerformanceStats g_stats;
static uint32_t g_stress_seed = 123456789U;

static int is_demo_mode(void) {
#if DEMO_MODE
    return 1;
#else
    return 0;
#endif
}

static int is_stress_mode(void) {
#if STRESS_MODE
    return 1;
#else
    return 0;
#endif
}

static uint32_t next_stress_random(void) {
    g_stress_seed = (1103515245U * g_stress_seed) + 12345U;
    return g_stress_seed;
}

static int should_inject_deadline_miss(uint32_t fish_id) {
    if (is_demo_mode()) {
        return fish_id == DEMO_DEADLINE_MISS_FISH_ID;
    }

    return fish_id % 25U == 0U;
}

static FishSample build_sample(uint32_t fish_id) {
    FishSample sample;

    sample.fish_id = fish_id;
    sample.timestamp_ns = 0;
    sample.class_id = FISH_INVALID;
    sample.error_code = ERR_NONE;

    if (is_demo_mode()) {
        switch (fish_id) {
            case 1:
                sample.length_mm = 180;
                sample.weight_g = 220;
                break;
            case 2:
                sample.length_mm = 280;
                sample.weight_g = 520;
                break;
            case 3:
                sample.length_mm = 420;
                sample.weight_g = 850;
                break;
            case 4:
                sample.length_mm = 0;
                sample.weight_g = 500;
                break;
            default:
                sample.length_mm = 340;
                sample.weight_g = 680;
                break;
        }
        return sample;
    }

    sample.length_mm = 100 + (uint16_t) (next_stress_random() % 401U);
    sample.weight_g = 100 + (uint16_t) (next_stress_random() % 801U);

    if (fish_id % 17U == 0U) {
        sample.length_mm = 0;
    } else if (fish_id % 19U == 0U) {
        sample.weight_g = 0;
    }

    return sample;
}

static void stats_init(PerformanceStats* stats) {
    stats->total_fish = 0;
    stats->deadline_ok_count = 0;
    stats->deadline_miss_count = 0;
    stats->invalid_data_count = 0;
    stats->total_latency_ms = 0.0;
    stats->max_latency_ms = 0.0;
    pthread_mutex_init(&stats->mutex, NULL);
}

static void stats_update(PerformanceStats* stats, const FishSample* sample, double latency_ms, int deadline_missed) {
    pthread_mutex_lock(&stats->mutex);

    stats->total_fish++;
    stats->total_latency_ms += latency_ms;
    if (latency_ms > stats->max_latency_ms) {
        stats->max_latency_ms = latency_ms;
    }

    if (deadline_missed) {
        stats->deadline_miss_count++;
    } else {
        stats->deadline_ok_count++;
    }

    if (sample->class_id == FISH_INVALID || sample->error_code == ERR_INVALID_DATA) {
        stats->invalid_data_count++;
    }

    pthread_mutex_unlock(&stats->mutex);
}

static void log_performance_summary(const PerformanceStats* stats) {
    char summary_line[256];
    double average_latency_ms = 0.0;
    int total_fish;
    int deadline_ok_count;
    int deadline_miss_count;
    int invalid_data_count;
    double max_latency_ms;

    pthread_mutex_lock((pthread_mutex_t*) &stats->mutex);

    total_fish = stats->total_fish;
    deadline_ok_count = stats->deadline_ok_count;
    deadline_miss_count = stats->deadline_miss_count;
    invalid_data_count = stats->invalid_data_count;
    max_latency_ms = stats->max_latency_ms;
    if (total_fish > 0) {
        average_latency_ms = stats->total_latency_ms / (double) total_fish;
    }

    pthread_mutex_unlock((pthread_mutex_t*) &stats->mutex);

    logger_log_message("[Performance Summary]");
    snprintf(
        summary_line,
        sizeof(summary_line),
        "total_fish=%d deadline_ok_count=%d deadline_miss_count=%d invalid_data_count=%d average_latency_ms=%.3f max_latency_ms=%.3f",
        total_fish,
        deadline_ok_count,
        deadline_miss_count,
        invalid_data_count,
        average_latency_ms,
        max_latency_ms
    );
    logger_log_message(summary_line);
}

static int create_thread_with_scheduler(
    pthread_t* thread,
    void* (*thread_func)(void*),
    void* arg,
    const char* thread_name,
    int policy,
    int priority
) {
    pthread_attr_t attr;
    struct sched_param sched_param;
    int result;

    printf(
        "[Main] scheduling policy attempt for %s: SCHED_FIFO priority=%d\n",
        thread_name,
        priority
    );

    result = pthread_attr_init(&attr);
    if (result != 0) {
        printf(
            "[Main] warning: could not init pthread attributes for %s (%s), running with default scheduler\n",
            thread_name,
            strerror(result)
        );
        return pthread_create(thread, NULL, thread_func, arg);
    }

    result = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    if (result == 0) {
        result = pthread_attr_setschedpolicy(&attr, policy);
    }
    if (result == 0) {
        sched_param.sched_priority = priority;
        result = pthread_attr_setschedparam(&attr, &sched_param);
    }

    if (result != 0) {
        printf(
            "[Main] warning: could not configure scheduler for %s (%s), running with default scheduler\n",
            thread_name,
            strerror(result)
        );
        pthread_attr_destroy(&attr);
        return pthread_create(thread, NULL, thread_func, arg);
    }

    result = pthread_create(thread, &attr, thread_func, arg);
    pthread_attr_destroy(&attr);

    if (result != 0) {
        printf(
            "[Main] warning: scheduler setup failed for %s (%s), permission denied or unsupported, running with default scheduler\n",
            thread_name,
            strerror(result)
        );
        return pthread_create(thread, NULL, thread_func, arg);
    }

    printf(
        "[Main] scheduling applied for %s: policy=SCHED_FIFO priority=%d\n",
        thread_name,
        priority
    );
    return 0;
}

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
    int i;

    (void) arg;

    for (i = 0; i < SAMPLE_COUNT; i++) {
        FishSample sample = build_sample((uint32_t) (i + 1));

        sample.timestamp_ns = get_timestamp_ns();
        sample.error_code = validate_fish_data(sample.length_mm, sample.weight_g)
            ? ERR_NONE
            : ERR_INVALID_DATA;

        logger_log_event(
            "Sensor produced",
            &sample,
            -1,
            "-",
            "-"
        );

        queue_push(&g_input_queue, sample);
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

        logger_log_event(
            "Classification consumed/classified",
            &sample,
            -1,
            "-",
            "-"
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
        const char* deadline_status;
        const char* destination;

        queue_pop(&g_output_queue, &sample);

        /* Intentional delay for deadline miss testing on a single sample. */
        if (should_inject_deadline_miss(sample.fish_id)) {
            usleep(TEST_DEADLINE_MISS_DELAY_US);
        }

        finish_timestamp_ns = get_timestamp_ns();
        latency_ns = finish_timestamp_ns - sample.timestamp_ns;
        latency_ms = (double) latency_ns / (double) NS_PER_MS;
        deadline_status = deadline_status_to_string(latency_ns);

        if (latency_ns > DEADLINE_NS) {
            sample.error_code = ERR_DEADLINE_MISS;
            destination = "LATE_REJECT";
            stats_update(&g_stats, &sample, latency_ms, 1);
        } else {
            destination = route_to_bin(sample.class_id);
            stats_update(&g_stats, &sample, latency_ms, 0);
        }

        logger_log_event(
            "Actuator routed",
            &sample,
            (long) latency_ns,
            deadline_status,
            destination
        );
        if (is_demo_mode()) {
            printf("[Actuator Thread] latency_ms=%.3f\n", latency_ms);
        }

        usleep(ACTUATOR_DELAY_US);
    }

    printf("[Actuator Thread] routing completed\n");
    return NULL;
}

int main(void) {
    pthread_t sensor_tid;
    pthread_t classifier_tid;
    pthread_t actuator_tid;
    int fifo_max_priority;
    int fifo_min_priority;
    int sensor_priority;
    int classification_priority;
    int actuator_priority;

    queue_init(&g_input_queue);
    queue_init(&g_output_queue);
    stats_init(&g_stats);
    logger_init("fish_system.log");
    logger_set_console_enabled(is_demo_mode());

    if (is_stress_mode()) {
        printf("[Main] starting STRESS_MODE batch simulation with %d fish\n", SAMPLE_COUNT);
        printf("[Main] detailed events will be written to fish_system.log\n");
    } else {
        printf("[Main] starting DEMO_MODE pipeline simulation\n");
    }

    fifo_max_priority = sched_get_priority_max(SCHED_FIFO);
    fifo_min_priority = sched_get_priority_min(SCHED_FIFO);

    if (fifo_max_priority == -1 || fifo_min_priority == -1) {
        printf("[Main] warning: could not query SCHED_FIFO priorities, using default scheduler\n");
        pthread_create(&sensor_tid, NULL, sensor_thread, NULL);
        pthread_create(&classifier_tid, NULL, classification_thread, NULL);
        pthread_create(&actuator_tid, NULL, actuator_thread, NULL);
    } else {
        sensor_priority = fifo_max_priority;
        classification_priority = fifo_min_priority + ((fifo_max_priority - fifo_min_priority) / 2);
        actuator_priority = fifo_max_priority - 1;

        create_thread_with_scheduler(
            &sensor_tid,
            sensor_thread,
            NULL,
            "Sensor Thread",
            SCHED_FIFO,
            sensor_priority
        );
        create_thread_with_scheduler(
            &classifier_tid,
            classification_thread,
            NULL,
            "Classification Thread",
            SCHED_FIFO,
            classification_priority
        );
        create_thread_with_scheduler(
            &actuator_tid,
            actuator_thread,
            NULL,
            "Actuator Thread",
            SCHED_FIFO,
            actuator_priority
        );
    }

    pthread_join(sensor_tid, NULL);
    pthread_join(classifier_tid, NULL);
    pthread_join(actuator_tid, NULL);

    log_performance_summary(&g_stats);
    logger_close();
    printf("[Main] simulation completed\n");
    return 0;
}
