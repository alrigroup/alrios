// C++20 Implementation with Vectorized Memory Array
#include <cstdint>
#include <array>

static std::array<uint32_t, 262144> memory_buffer{}; // 1MB Heap/Linear Buffer

extern "C" uint32_t compute(uint32_t iters) noexcept {
    uint32_t acc = 0x12345678U;
    for (uint32_t i = 0; i < iters; ++i) {
        uint32_t idx = i & 0x3FFFFU;
        uint32_t val = memory_buffer[idx];
        acc = ((acc ^ (i + 0x9E3779B9U) ^ val) * 1664525U) + 1013904223U;
        memory_buffer[idx] = acc;
    }
    return acc;
}
