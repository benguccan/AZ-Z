#ifndef LOGGER_H
#define LOGGER_H

#include "fish_system.h"

void logger_init(const char* filename);
void logger_close(void);
void logger_set_console_enabled(int enabled);
void logger_log_event(
    const char* stage,
    const FishSample* sample,
    long latency_ns,
    const char* deadline_status,
    const char* destination
);
void logger_log_message(const char* message);

#endif
