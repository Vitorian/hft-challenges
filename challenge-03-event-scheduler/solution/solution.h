#pragma once
// Challenge 03: Event Scheduler — Naive Reference Implementation
// This is correct but slow. You can do much better!

#include "scheduler.h"
#include <map>
#include <unordered_map>
#include <climits>

namespace hftu {

// Your EventScheduler must support:
//
//   schedule(event_id, time_us)
//     Schedule a one-shot event at the given absolute time (microseconds).
//     If event_id is already scheduled, replace it (implicit cancel + reschedule).
//
//   cancel(event_id) -> bool
//     Cancel a pending event. Returns true if found and cancelled.
//
//   advance(new_time_us, callback, user_data) -> uint32_t
//     Advance the clock to new_time_us. Fire all events with time <= new_time_us
//     by calling callback(event_id, scheduled_time, user_data).
//     Time always advances monotonically (new_time_us >= previous new_time_us).
//     Events at the same microsecond may fire in any order.
//     Returns number of events fired.
//
//   size() -> uint64_t
//     Number of pending events.
//
//   next_event_time() -> int64_t
//     Time of next event, or INT64_MAX if empty.
//
class EventScheduler {
public:
    EventScheduler() = default;
    ~EventScheduler() = default;

    void schedule(uint64_t event_id, int64_t time_us);
    bool cancel(uint64_t event_id);
    uint32_t advance(int64_t new_time_us, EventCallback cb, void* user_data);
    uint64_t size() const;
    int64_t next_event_time() const;

private:
    // time -> event_ids at that time
    std::multimap<int64_t, uint64_t> timeline_;
    // event_id -> iterator into timeline_ (for cancel)
    std::unordered_map<uint64_t, std::multimap<int64_t, uint64_t>::iterator> lookup_;
};

} // namespace hftu
