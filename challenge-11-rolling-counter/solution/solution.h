#pragma once
// Challenge 11: Rolling Counter
// Edit this file and solution.cpp to implement your solution.
//
// Maintain a sliding window count of events. The precision parameter
// allows approximate counting — you don't need nanosecond-exact expiration.

#include <cstdint>
#include <cstddef>
#include <deque>

namespace hftu {

class RollingCounter {
public:
    // interval_ns: window size in nanoseconds
    // precision_ns: acceptable error window in nanoseconds
    RollingCounter(int64_t interval_ns, int64_t precision_ns);

    // Advance clock. time_ns is guaranteed monotonically increasing.
    void update(int64_t time_ns);

    // Add count events at the current time.
    void addEvent(size_t count);

    // Return total events in the rolling window.
    size_t count() const;
    operator size_t() const;

private:
    int64_t interval_ns_;
    int64_t current_time_ = 0;
    size_t total_ = 0;
    struct Entry { int64_t time; size_t count; };
    std::deque<Entry> events_;
};

} // namespace hftu
