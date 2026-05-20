#include "logger.h"

#include <string.h>
#include <pthread.h>
#include <stdio.h>

static FILE* g_log_file = NULL;
static pthread_mutex_t g_logger_mutex = PTHREAD_MUTEX_INITIALIZER;
static int g_console_enabled = 1;

void logger_init(const char* filename) {
    pthread_mutex_lock(&g_logger_mutex);

    if (g_log_file != NULL) {
        fclose(g_log_file);
        g_log_file = NULL;
    }

    if (filename != NULL) {
        g_log_file = fopen(filename, "w");
    }

    pthread_mutex_unlock(&g_logger_mutex);
}

void logger_close(void) {
    pthread_mutex_lock(&g_logger_mutex);

    if (g_log_file != NULL) {
        fclose(g_log_file);
        g_log_file = NULL;
    }

    pthread_mutex_unlock(&g_logger_mutex);
}

void logger_set_console_enabled(int enabled) {
    pthread_mutex_lock(&g_logger_mutex);
    g_console_enabled = enabled != 0;
    pthread_mutex_unlock(&g_logger_mutex);
}

void logger_log_event(
    const char* stage,
    const FishSample* sample,
    long latency_ns,
    const char* deadline_status,
    const char* destination
) {
    const char* safe_stage = stage != NULL ? stage : "UNKNOWN_STAGE";
    const char* safe_deadline_status = deadline_status != NULL ? deadline_status : "-";
    const char* safe_destination = destination != NULL ? destination : "-";
    const char* class_text = "UNCLASSIFIED";

    pthread_mutex_lock(&g_logger_mutex);

    if (sample == NULL) {
        if (g_console_enabled) {
            printf("[%s] no sample data\n", safe_stage);
        }
        if (g_log_file != NULL) {
            fprintf(g_log_file, "[%s] no sample data\n", safe_stage);
            fflush(g_log_file);
        }
        pthread_mutex_unlock(&g_logger_mutex);
        return;
    }

    if (stage == NULL || strcmp(safe_stage, "Sensor produced") != 0) {
        class_text = fish_class_to_string(sample->class_id);
    }

    if (g_console_enabled) {
        printf(
            "[%s] fish_id=%u length=%umm weight=%ug class=%s error=%s latency_ns=%ld deadline=%s destination=%s\n",
            safe_stage,
            sample->fish_id,
            sample->length_mm,
            sample->weight_g,
            class_text,
            error_code_to_string(sample->error_code),
            latency_ns,
            safe_deadline_status,
            safe_destination
        );
    }

    if (g_log_file != NULL) {
        fprintf(
            g_log_file,
            "[%s] fish_id=%u length=%umm weight=%ug class=%s error=%s latency_ns=%ld deadline=%s destination=%s\n",
            safe_stage,
            sample->fish_id,
            sample->length_mm,
            sample->weight_g,
            class_text,
            error_code_to_string(sample->error_code),
            latency_ns,
            safe_deadline_status,
            safe_destination
        );
        fflush(g_log_file);
    }

    pthread_mutex_unlock(&g_logger_mutex);
}

void logger_log_message(const char* message) {
    const char* safe_message = message != NULL ? message : "[Logger] empty message";

    pthread_mutex_lock(&g_logger_mutex);

    printf("%s\n", safe_message);
    if (g_log_file != NULL) {
        fprintf(g_log_file, "%s\n", safe_message);
        fflush(g_log_file);
    }

    pthread_mutex_unlock(&g_logger_mutex);
}
