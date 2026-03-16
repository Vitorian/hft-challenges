// Public benchmark for Challenge 03: Event Scheduler
//
// Workload: millions of events with varying time horizons, mixed
// schedule/cancel/advance operations. Measures per-operation P99 latency.
//
// Invariants:
//   - advance() is always called with monotonically increasing time
//   - Events at the same microsecond may fire in any order
//   - event_id values are unique across the lifetime of the scheduler

#include "common/benchmark_harness.h"
#include "solution/solution.h"

#include <algorithm>
#include <cstdio>
#include <random>
#include <vector>

namespace {

// ---------------------------------------------------------------------------
// Operation types
// ---------------------------------------------------------------------------

struct Operation {
    enum Type : uint8_t {
        SCHEDULE,
        CANCEL,
        ADVANCE,
        QUERY_SIZE,
        QUERY_NEXT,
    };
    Type type;
    uint64_t event_id;
    int64_t time_us;
};

// ---------------------------------------------------------------------------
// Noop callback — just counts
// ---------------------------------------------------------------------------

void noop_callback(uint64_t, int64_t, void* user) {
    (*static_cast<uint64_t*>(user))++;
}

// ---------------------------------------------------------------------------
// Workload config
// ---------------------------------------------------------------------------

struct WorkloadConfig {
    size_t num_events = 500'000;      // events to prefill
    size_t timed_ops = 200'000;       // operations to measure
    int64_t time_horizon_us = 60'000'000;  // 60 seconds in microseconds
    int64_t advance_step_us = 1'000;  // advance 1ms at a time
    uint64_t seed = 42;
};

struct GeneratedWorkload {
    std::vector<Operation> setup;     // prefill events (all SCHEDULE)
    std::vector<Operation> timed;     // measured operations
    int64_t start_time_us;
};

// ---------------------------------------------------------------------------
// Workload generation
// ---------------------------------------------------------------------------

GeneratedWorkload generate_workload(const WorkloadConfig& cfg) {
    std::mt19937_64 gen(cfg.seed);
    GeneratedWorkload wl;
    wl.start_time_us = 1'000'000;  // start at 1 second

    // Time distribution for new events:
    //   50% within 1ms (near-term, hot)
    //   30% within 100ms (medium)
    //   20% within full horizon (far future)
    std::uniform_int_distribution<int64_t> near_dist(1, 1'000);
    std::uniform_int_distribution<int64_t> mid_dist(1'000, 100'000);
    std::uniform_int_distribution<int64_t> far_dist(100'000, cfg.time_horizon_us);
    std::uniform_real_distribution<double> pct(0.0, 1.0);

    auto pick_offset = [&]() -> int64_t {
        double r = pct(gen);
        if (r < 0.5) return near_dist(gen);
        if (r < 0.8) return mid_dist(gen);
        return far_dist(gen);
    };

    // --- Prefill ---
    wl.setup.reserve(cfg.num_events);
    for (size_t i = 0; i < cfg.num_events; ++i) {
        wl.setup.push_back({Operation::SCHEDULE,
                            static_cast<uint64_t>(i + 1),
                            wl.start_time_us + pick_offset()});
    }

    // --- Timed operations ---
    wl.timed.reserve(cfg.timed_ops);
    uint64_t next_event_id = cfg.num_events + 1;
    int64_t current_time = wl.start_time_us;
    std::vector<uint64_t> live_ids;
    live_ids.reserve(cfg.num_events);
    for (size_t i = 0; i < cfg.num_events; ++i)
        live_ids.push_back(i + 1);

    std::uniform_int_distribution<int> op_dist(0, 99);

    for (size_t i = 0; i < cfg.timed_ops; ++i) {
        int r = op_dist(gen);

        if (r < 35) {
            // SCHEDULE
            uint64_t eid = next_event_id++;
            wl.timed.push_back({Operation::SCHEDULE, eid, current_time + pick_offset()});
            live_ids.push_back(eid);

        } else if (r < 55 && live_ids.size() > 1000) {
            // CANCEL
            std::uniform_int_distribution<size_t> idx(0, live_ids.size() - 1);
            size_t ci = idx(gen);
            wl.timed.push_back({Operation::CANCEL, live_ids[ci], 0});
            live_ids[ci] = live_ids.back();
            live_ids.pop_back();

        } else if (r < 75) {
            // ADVANCE — monotonically increasing
            current_time += cfg.advance_step_us;
            wl.timed.push_back({Operation::ADVANCE, 0, current_time});

        } else if (r < 90) {
            // QUERY_SIZE
            wl.timed.push_back({Operation::QUERY_SIZE, 0, 0});

        } else {
            // QUERY_NEXT
            wl.timed.push_back({Operation::QUERY_NEXT, 0, 0});
        }
    }

    return wl;
}

// ---------------------------------------------------------------------------
// Latency stats
// ---------------------------------------------------------------------------

struct LatencyStats {
    uint64_t p50, p99, p999, max;
    double avg;
    size_t count;
};

LatencyStats compute_stats(std::vector<uint64_t>& lat) {
    if (lat.empty()) return {};
    std::sort(lat.begin(), lat.end());
    size_t n = lat.size();
    uint64_t sum = 0;
    for (auto l : lat) sum += l;
    return {
        lat[n * 50 / 100],
        lat[std::min(n - 1, n * 99 / 100)],
        lat[std::min(n - 1, n * 999 / 1000)],
        lat.back(),
        static_cast<double>(sum) / static_cast<double>(n),
        n,
    };
}

// ---------------------------------------------------------------------------
// Execute one operation
// ---------------------------------------------------------------------------

inline void execute_op(hftu::EventScheduler& sched, const Operation& op,
                       uint64_t& fire_count) {
    switch (op.type) {
        case Operation::SCHEDULE:
            sched.schedule(op.event_id, op.time_us);
            break;
        case Operation::CANCEL:
            hftu::do_not_optimize(sched.cancel(op.event_id));
            break;
        case Operation::ADVANCE: {
            uint32_t n = sched.advance(op.time_us, noop_callback, &fire_count);
            hftu::do_not_optimize(n);
            break;
        }
        case Operation::QUERY_SIZE:
            hftu::do_not_optimize(sched.size());
            break;
        case Operation::QUERY_NEXT:
            hftu::do_not_optimize(sched.next_event_time());
            break;
    }
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Benchmark
// ---------------------------------------------------------------------------

static hftu::RegisterBenchmark reg_solution(
    "BM_Solution", 100'000,
    [](int iterations) -> uint64_t {
        WorkloadConfig cfg;
        const auto wl = generate_workload(cfg);
        std::vector<uint64_t> all_latencies;
        all_latencies.reserve(wl.timed.size() * static_cast<size_t>(iterations));

        for (int iter = 0; iter < iterations; ++iter) {
            hftu::EventScheduler sched;
            uint64_t fire_count = 0;

            // Prefill
            for (const auto& op : wl.setup)
                sched.schedule(op.event_id, op.time_us);

            // Timed phase
            for (const auto& op : wl.timed) {
                uint64_t t0 = hftu::cycle_start();
                execute_op(sched, op, fire_count);
                hftu::clobber();
                uint64_t t1 = hftu::cycle_end();
                all_latencies.push_back(t1 - t0);
            }
        }

        auto stats = compute_stats(all_latencies);
        std::fprintf(stderr, "  Latency (cycles): p50=%lu  p99=%lu  p999=%lu  max=%lu  avg=%.0f\n",
                     stats.p50, stats.p99, stats.p999, stats.max, stats.avg);

        // Return total so cycles_per_op = p99
        return stats.p99 * static_cast<uint64_t>(iterations) * wl.timed.size();
    }
);

int main() {
    hftu::run_benchmarks();
    return 0;
}
