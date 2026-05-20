#ifndef FISH_SYSTEM_H
#define FISH_SYSTEM_H

#include <stdint.h>

typedef enum {
    FISH_SMALL = 0,
    FISH_MEDIUM = 1,
    FISH_LARGE = 2,
    FISH_INVALID = 3
} FishClass;

typedef enum {
    ERR_NONE = 0,
    ERR_DEADLINE_MISS,
    ERR_SENSOR_READ_FAILURE,
    ERR_INVALID_DATA,
    ERR_QUEUE_OVERFLOW,
    ERR_ACTUATOR_DELAY
} ErrorCode;

typedef struct {
    uint32_t fish_id;
    uint16_t length_mm;
    uint16_t weight_g;
    uint64_t timestamp_ns;
    FishClass class_id;
    ErrorCode error_code;
} FishSample;

const char* fish_class_to_string(FishClass class_id);
const char* error_code_to_string(ErrorCode error);
FishClass classify_fish(uint16_t length_mm, uint16_t weight_g);
int validate_fish_data(uint16_t length_mm, uint16_t weight_g);

#endif
