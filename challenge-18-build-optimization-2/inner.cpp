#include "inner.h"

extern "C" __attribute__((noinline))
uint64_t inner(uint64_t x, uint64_t y) {
    return x * 0x9E3779B97F4A7C15ULL + y + 0x12345678ULL;
}
