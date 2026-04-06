#pragma once
#include <cstdint>
#include <cstddef>
#include <map>

namespace hftu {

struct Message {
    uint64_t sequence;      // 8
    uint64_t timestamp;     // 8
    int64_t  price;         // 8
    uint32_t order_id;      // 4
    int32_t  quantity;      // 4
    uint16_t symbol_id;     // 2
    uint8_t  msg_type;      // 1
    uint8_t  side;          // 1
    uint8_t  _pad[8];      // 36 + 4 implicit + 8 = 48 total
};
static_assert(sizeof(Message) == 48, "Message must be 48 bytes");

class FeedArbiter {
public:
    FeedArbiter();

    /// Called once with the maximum expected sequence number. NOT timed.
    void build(uint64_t max_sequence);

    /// A message arrived on one of the feeds (A or B carry identical data).
    /// Deduplicate and emit in sequence-number order.
    /// Returns the number of in-order messages written to `out`.
    /// `out` is pre-allocated with sufficient capacity.
    size_t on_message(const Message& msg, Message* out);

private:
    uint64_t next_expected_ = 1;
    std::map<uint64_t, Message> buffer_;
};

} // namespace hftu
