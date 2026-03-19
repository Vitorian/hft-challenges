#pragma once
// Challenge 08: Ticker Lookup
// Edit this file and solution.cpp to implement your solution.
//
// You receive all keys once via build(), then only find() is called.
// build() is NOT timed — spend as long as you want preprocessing.

#include <cstdint>
#include <cstddef>
#include <string>
#include <unordered_map>

namespace hftu {

struct TickerEntry {
    const char* symbol;    // null-terminated, max 6 chars
    size_t symbol_len;
    uint32_t value;
};

class TickerLookup {
public:
    TickerLookup();

    // Receive all entries at once. Called exactly once. NOT timed.
    void build(const TickerEntry* entries, size_t count);

    // Look up a symbol. Returns pointer to value, or nullptr if not found.
    const uint32_t* find(const char* symbol, size_t symbol_len) const;

private:
    std::unordered_map<std::string, uint32_t> map_;
};

} // namespace hftu
