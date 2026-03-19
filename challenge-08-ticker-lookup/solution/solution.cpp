// Challenge 08: Ticker Lookup — Skeleton Implementation
// This is a correct but slow std::unordered_map reference. You can do MUCH better!
// Since build() is not timed, you could build a perfect hash, a trie, or any
// precomputed structure.

#include "solution.h"

namespace hftu {

TickerLookup::TickerLookup() {}

void TickerLookup::build(const TickerEntry* entries, size_t count) {
    map_.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        map_.emplace(std::string(entries[i].symbol, entries[i].symbol_len), entries[i].value);
    }
}

const uint32_t* TickerLookup::find(const char* symbol, size_t symbol_len) const {
    auto it = map_.find(std::string(symbol, symbol_len));
    if (it == map_.end()) return nullptr;
    return &it->second;
}

} // namespace hftu
