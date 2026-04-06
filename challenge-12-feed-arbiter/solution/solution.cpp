#include "solution.h"

namespace hftu {

FeedArbiter::FeedArbiter() = default;

void FeedArbiter::build(uint64_t /*max_sequence*/) {
    next_expected_ = 1;
    buffer_.clear();
}

size_t FeedArbiter::on_message(const Message& msg, Message* out) {
    uint64_t seq = msg.sequence;

    // Duplicate or already emitted
    if (seq < next_expected_)
        return 0;

    // Already buffered
    if (buffer_.count(seq))
        return 0;

    // Future message — buffer it
    if (seq > next_expected_) {
        buffer_[seq] = msg;
        return 0;
    }

    // seq == next_expected_ — emit it and flush any contiguous buffered
    size_t count = 0;
    out[count++] = msg;
    next_expected_++;

    while (!buffer_.empty()) {
        auto it = buffer_.begin();
        if (it->first != next_expected_)
            break;
        out[count++] = it->second;
        buffer_.erase(it);
        next_expected_++;
    }

    return count;
}

} // namespace hftu
