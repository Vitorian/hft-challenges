#pragma once
// Challenge 20: Fast Integer-to-String
// Edit this file and solution.cpp to implement your solution.
//
// Convert a uint64_t to its decimal ASCII representation as fast as
// possible. This is the hot path of every FIX encoder and order gateway:
// prices, quantities, and sequence numbers all leave the process as
// decimal text.

#include <cstdint>
#include <cstddef>

namespace hftu {

// Write the decimal representation of `value` into `buf` and return the
// number of characters written (1..20).
//
// Contract:
//   - Digits only, no leading zeros ("0" for zero), no sign.
//   - No null terminator required.
//   - buf always has 32 writable bytes. Bytes beyond the returned length
//     are scratch — write whatever you like there.
//   - Output is validated byte-exactly against a reference, including
//     edge cases (0, UINT64_MAX, every power of ten).
//
// You may implement it here inline or in solution.cpp — your choice.
size_t u64_to_chars(uint64_t value, char* buf);

} // namespace hftu
