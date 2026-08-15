// C Implementation with Direct Pointer Linear Memory Operations
#include <stdint.h>

static uint32_t memory_buffer[262144]; // 1MB Memory Array

uint32_t compute(uint32_t iters) {
    uint32_t acc = 0x12345678;
    for (uint32_t i = 0; i < iters; i++) {
        uint32_t idx = i & 0x3FFFF;
        uint32_t val = memory_buffer[idx];
        acc = ((acc ^ (i + 0x9E3779B9) ^ val) * 1664525) + 1013904223;
        memory_buffer[idx] = acc;
    }
    return acc;
}
