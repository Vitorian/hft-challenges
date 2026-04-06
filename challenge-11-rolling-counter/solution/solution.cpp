// Challenge 11: Rolling Counter — Skeleton Implementation
// This is a correct but slow deque-based reference. You can do MUCH better!
// The precision parameter is ignored here — this tracks exact timestamps.
// A smarter solution would use buckets of size ~precision_ns.

#include "solution.h"

namespace hftu {

RollingCounter::RollingCounter(int64_t interval_ns, int64_t /*precision_ns*/)
    : interval_ns_(interval_ns) {}

void RollingCounter::update(int64_t time_ns) {
    current_time_ = time_ns;
    int64_t cutoff = time_ns - interval_ns_;
    while (!events_.empty() && events_.front().time <= cutoff) {
        total_ -= events_.front().count;
        events_.pop_front();
    }
}

void RollingCounter::addEvent(size_t count) {
    events_.push_back({current_time_, count});
    total_ += count;
}

size_t RollingCounter::count() const {
    return total_;
}

RollingCounter::operator size_t() const {
    return total_;
}

} // namespace hftu
