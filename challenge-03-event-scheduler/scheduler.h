#pragma once
// Event Scheduler types for Challenge 03.
// Do NOT modify this file — it will be overwritten during certified runs.
//
// You must implement an EventScheduler class (in solution/solution.h) that can
// handle millions of pending events with varying time horizons. Events are
// scheduled at absolute microsecond timestamps and fired when time is advanced.
//
// The key metric is P99 latency — your scheduler must not have expensive
// worst-case operations that would disrupt a real-time system.

#include <cstdint>

namespace hftu {

// Called when an event fires during advance().
//   event_id:       the ID passed to schedule()
//   scheduled_time: the time the event was scheduled for (microseconds)
//   user_data:      opaque pointer passed to advance()
using EventCallback = void(*)(uint64_t event_id, int64_t scheduled_time, void* user_data);

} // namespace hftu
