#pragma once
// Challenge 10: FIX Encoder
// Edit this file and solution.cpp to implement your solution.
//
// build() gives you a template message. encode() must produce a valid FIX message
// with updated fields as fast as possible. Pre-compute everything you can in build().

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace hftu {

struct OrderFields {
    int64_t  timestamp;    // tag 52: nanoseconds from epoch
    uint64_t seq_num;      // tag 34
    uint64_t cl_ord_id;    // tag 11
    int64_t  price;        // tag 44: fixed-point * 10^8
    int64_t  quantity;     // tag 38
    int8_t   side;         // tag 54: 1=Buy, 2=Sell
};

class FixBuilder {
public:
    FixBuilder();

    // Receive template as tag-value pairs. NOT timed.
    void build(std::span<const std::pair<int, std::string>> fields);

    // Produce a valid FIX message with the given order fields.
    // Returned string_view valid until the next encode() call.
    std::string_view encode(const OrderFields& order);

private:
    std::vector<std::pair<int, std::string>> template_fields_;
    std::string buffer_;
};

} // namespace hftu
