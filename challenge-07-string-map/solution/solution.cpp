// Challenge 07: String Map — Skeleton Implementation
// This is a correct but slow std::unordered_map reference. You can do MUCH better!

#include "solution.h"

namespace hftu {

StringMap::StringMap() {}

void StringMap::insert(const char* key, size_t key_len, uint32_t value) {
    map_.emplace(std::string(key, key_len), value);
}

const uint32_t* StringMap::find(const char* key, size_t key_len) const {
    auto it = map_.find(std::string(key, key_len));
    if (it == map_.end()) return nullptr;
    return &it->second;
}

} // namespace hftu
