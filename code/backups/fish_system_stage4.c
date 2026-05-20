#include "fish_system.h"

const char* fish_class_to_string(FishClass class_id) {
    switch (class_id) {
        case FISH_SMALL:
            return "SMALL";
        case FISH_MEDIUM:
            return "MEDIUM";
        case FISH_LARGE:
            return "LARGE";
        case FISH_INVALID:
            return "INVALID";
        default:
            return "UNKNOWN_CLASS";
    }
}

const char* error_code_to_string(ErrorCode error) {
    switch (error) {
        case ERR_NONE:
            return "ERR_NONE";
        case ERR_DEADLINE_MISS:
            return "ERR_DEADLINE_MISS";
        case ERR_SENSOR_READ_FAILURE:
            return "ERR_SENSOR_READ_FAILURE";
        case ERR_INVALID_DATA:
            return "ERR_INVALID_DATA";
        case ERR_QUEUE_OVERFLOW:
            return "ERR_QUEUE_OVERFLOW";
        case ERR_ACTUATOR_DELAY:
            return "ERR_ACTUATOR_DELAY";
        default:
            return "ERR_UNKNOWN";
    }
}

int validate_fish_data(uint16_t length_mm, uint16_t weight_g) {
    return length_mm != 0 && weight_g != 0;
}

FishClass classify_fish(uint16_t length_mm, uint16_t weight_g) {
    if (!validate_fish_data(length_mm, weight_g)) {
        return FISH_INVALID;
    }

    if (length_mm < 200 || weight_g < 250) {
        return FISH_SMALL;
    }

    if (length_mm >= 350 || weight_g >= 700) {
        return FISH_LARGE;
    }

    return FISH_MEDIUM;
}
