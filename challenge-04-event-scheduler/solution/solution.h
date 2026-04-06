#pragma once
// Challenge 04: Event Scheduler (No Cancel) — Skeleton
//
// Intrusive events + CRTP. You own the Event struct — modify it freely.
// This skeleton uses std::multimap. It is correct but slow.
// You can do MUCH better.

#include <cstdint>
#include <climits>
#include <map>

namespace hftu {

// ---- Modify this struct freely ----
// Add intrusive pointers, bucket indices, timestamps — whatever you need.
struct Event {};

// ---- Skeleton: std::multimap (correct, O(log N) insert) ----
template <typename Derived>
class EventScheduler {
public:
    EventScheduler() = default;

    Derived* me() { return static_cast<Derived*>(this); }

    void schedule(Event* event, int64_t time_ns) {
        map_.emplace(time_ns, event);
        ++size_;
    }

    uint32_t advance(int64_t new_time_ns) {
        uint32_t fired = 0;
        while (!map_.empty()) {
            auto it = map_.begin();
            if (it->first > new_time_ns) break;
            Event* e = it->second;
            int64_t t = it->first;
            map_.erase(it);
            --size_;
            me()->fire(e, t);
            ++fired;
        }
        return fired;
    }

    uint64_t size() const { return size_; }

    int64_t next_event_time() const {
        return map_.empty() ? INT64_MAX : map_.begin()->first;
    }

private:
    std::multimap<int64_t, Event*> map_;
    uint64_t size_ = 0;
};

} // namespace hftu
