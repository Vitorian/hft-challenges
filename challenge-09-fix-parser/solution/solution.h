#pragma once
// Challenge 09: FIX Parser
// Edit this file and solution.cpp to implement your solution.

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace hftu {

struct TickerEntry {
    std::string_view symbol;
    uint32_t value;
};

struct ParsedOrder {
    char     msg_type;    // 'D', '8', 'F', 'G'
    int8_t   side;        // 1=Buy, 2=Sell
    uint32_t symbol_id;   // from build() mapping
    int64_t  timestamp;   // tag 52: nanoseconds from epoch
    int64_t  price;       // price * 100000000 (8 decimal places)
    int64_t  quantity;
    bool     valid;       // false if checksum failed
};

class FixParser {
public:
    FixParser();

    // Receive symbol → ID mapping. NOT timed.
    void build(std::span<const TickerEntry> entries);

    // Parse a batch of concatenated FIX messages, appending results to out.
    // out.capacity() is guaranteed to be large enough for all messages in data.
    // You should out.push_back() or out.emplace_back() for each message.
    void parse_batch(std::string_view data, std::vector<ParsedOrder>& out);

private:
    std::unordered_map<std::string, uint32_t> symbol_map_;
};

} // namespace hftu
