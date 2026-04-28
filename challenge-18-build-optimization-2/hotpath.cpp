#include "hotpath.h"
#include "inner.h"

extern "C" __attribute__((noinline, visibility("default")))
uint64_t mix(uint64_t x, uint64_t y) {
    return inner(x, y);
}
