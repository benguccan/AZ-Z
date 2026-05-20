#include <stdio.h>

#include "fish_system.h"

int main() {
    printf("uint32_t: %zu byte\n", sizeof(uint32_t));
    printf("uint16_t: %zu byte\n", sizeof(uint16_t));
    printf("uint64_t: %zu byte\n", sizeof(uint64_t));
    printf("FishClass: %zu byte\n", sizeof(FishClass));
    printf("ErrorCode: %zu byte\n", sizeof(ErrorCode));
    printf("FishSample: %zu byte\n", sizeof(FishSample));
    return 0;
}
