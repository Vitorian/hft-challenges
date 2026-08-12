// Challenge 20: Fast Integer-to-String — Skeleton Implementation
// This is a correct but slow snprintf-based reference. You can do MUCH better!
// Hints to explore: two-digits-at-a-time lookup tables, computing the digit
// count up front, branchless length selection, writing back-to-front.

#include "solution.h"

#include <cinttypes>
#include <cstdio>

namespace hftu {

size_t u64_to_chars(uint64_t value, char* buf) {
    int n = std::snprintf(buf, 21, "%" PRIu64, value);
    return static_cast<size_t>(n);
}

} // namespace hftu
