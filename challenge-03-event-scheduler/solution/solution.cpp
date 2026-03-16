// Challenge 03: Event Scheduler — Naive Reference Implementation
// Uses std::multimap for time ordering and unordered_map for O(log n) cancel.
// This is correct but has O(log n) schedule and O(log n) cancel.

#include "solution.h"

namespace hftu {

void EventScheduler::schedule(uint64_t event_id, int64_t time_us) {
    // Implicit cancel if already scheduled
    auto it = lookup_.find(event_id);
    if (it != lookup_.end()) {
        timeline_.erase(it->second);
        lookup_.erase(it);
    }

    auto pos = timeline_.emplace(time_us, event_id);
    lookup_[event_id] = pos;
}

bool EventScheduler::cancel(uint64_t event_id) {
    auto it = lookup_.find(event_id);
    if (it == lookup_.end()) return false;
    timeline_.erase(it->second);
    lookup_.erase(it);
    return true;
}

uint32_t EventScheduler::advance(int64_t new_time_us, EventCallback cb, void* user_data) {
    uint32_t fired = 0;
    while (!timeline_.empty()) {
        auto it = timeline_.begin();
        if (it->first > new_time_us) break;

        uint64_t event_id = it->second;
        int64_t scheduled_time = it->first;

        // Remove from both structures before callback
        lookup_.erase(event_id);
        timeline_.erase(it);

        cb(event_id, scheduled_time, user_data);
        ++fired;
    }
    return fired;
}

uint64_t EventScheduler::size() const {
    return lookup_.size();
}

int64_t EventScheduler::next_event_time() const {
    if (timeline_.empty()) return INT64_MAX;
    return timeline_.begin()->first;
}

} // namespace hftu
