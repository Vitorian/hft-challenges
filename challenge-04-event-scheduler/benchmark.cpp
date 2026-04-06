// Public benchmark for Challenge 04: Timer Wheel
//
// Intrusive event scheduler with CRTP fire(). No cancel.
// Measures per-operation P99 latency across mixed schedule/advance workloads.

#include "common/benchmark_harness.h"
#include "solution/solution.h"

#include <algorithm>
#include <cstdio>
#include <random>
#include <vector>

namespace {

// Concrete scheduler that counts fires
struct TestScheduler : public hftu::EventScheduler<TestScheduler> {
    uint64_t fire_count = 0;
    void fire(hftu::Event*, int64_t) { ++fire_count; }
};

struct Operation {
    enum Type : uint8_t { SCHEDULE, ADVANCE, QUERY_SIZE, QUERY_NEXT };
    Type type;
    int64_t time_ns;
    uint32_t event_idx;  // index into event pool
};

struct Workload {
    std::vector<Operation> setup;
    std::vector<Operation> timed;
    std::vector<hftu::Event> events;
    int64_t start_time_ns;
};

Workload generate_workload(uint64_t seed = 42) {
    std::mt19937_64 gen(seed);
    Workload wl;
    wl.start_time_ns = 1'000'000'000LL;  // 1 second in ns

    constexpr size_t NUM_EVENTS = 500'000;
    constexpr size_t TIMED_OPS = 200'000;

    wl.events.resize(NUM_EVENTS);

    // Time distributions
    std::uniform_int_distribution<int64_t> near_dist(1, 1'000'000);          // 1μs
    std::uniform_int_distribution<int64_t> mid_dist(1'000'000, 100'000'000); // 100ms
    std::uniform_int_distribution<int64_t> far_dist(100'000'000, 60'000'000'000LL); // 60s
    std::uniform_real_distribution<double> pct(0.0, 1.0);

    auto pick_offset = [&]() -> int64_t {
        double r = pct(gen);
        if (r < 0.5) return near_dist(gen);
        if (r < 0.8) return mid_dist(gen);
        return far_dist(gen);
    };

    // Prefill
    wl.setup.reserve(NUM_EVENTS);
    for (uint32_t i = 0; i < NUM_EVENTS; ++i) {
        wl.setup.push_back({Operation::SCHEDULE,
                            wl.start_time_ns + pick_offset(), i});
    }

    // Timed operations
    wl.timed.reserve(TIMED_OPS);
    int64_t current_time = wl.start_time_ns;
    uint32_t next_event = NUM_EVENTS;  // for reuse after fire
    std::vector<uint32_t> free_events;
    free_events.reserve(NUM_EVENTS);

    std::uniform_int_distribution<int> op_dist(0, 99);
    std::uniform_int_distribution<int64_t> advance_step(1'000, 1'000'000); // 1μs-1ms

    for (size_t i = 0; i < TIMED_OPS; ++i) {
        int r = op_dist(gen);

        if (r < 40 && !free_events.empty()) {
            // SCHEDULE a reused event
            uint32_t idx = free_events.back();
            free_events.pop_back();
            wl.timed.push_back({Operation::SCHEDULE,
                                current_time + pick_offset(), idx});
        } else if (r < 70) {
            // ADVANCE
            current_time += advance_step(gen);
            wl.timed.push_back({Operation::ADVANCE, current_time, 0});
        } else if (r < 85) {
            // QUERY_SIZE
            wl.timed.push_back({Operation::QUERY_SIZE, 0, 0});
        } else {
            // QUERY_NEXT
            wl.timed.push_back({Operation::QUERY_NEXT, 0, 0});
        }
    }

    return wl;
}

struct LatencyStats {
    uint64_t p50, p99, p999, max;
    double avg;
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
    };
}

static hftu::RegisterBenchmark reg_solution(
    "BM_Solution", 100'000,
    [](int iterations) -> uint64_t {
        auto wl = generate_workload();
        std::vector<uint64_t> all_latencies;
        all_latencies.reserve(wl.timed.size() * iterations);

        for (int iter = 0; iter < iterations; ++iter) {
            TestScheduler sched;
            sched.fire_count = 0;

            // Prefill
            for (const auto& op : wl.setup)
                sched.schedule(&wl.events[op.event_idx], op.time_ns);

            // Timed phase
            for (const auto& op : wl.timed) {
                uint64_t t0 = hftu::cycle_start();
                switch (op.type) {
                    case Operation::SCHEDULE:
                        sched.schedule(&wl.events[op.event_idx], op.time_ns);
                        break;
                    case Operation::ADVANCE: {
                        uint32_t n = sched.advance(op.time_ns);
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
                hftu::clobber();
                uint64_t t1 = hftu::cycle_end();
                all_latencies.push_back(t1 - t0);
            }
        }

        auto stats = compute_stats(all_latencies);
        std::fprintf(stderr, "  Latency (cycles): p50=%lu  p99=%lu  p999=%lu  max=%lu  avg=%.0f\n",
                     stats.p50, stats.p99, stats.p999, stats.max, stats.avg);

        return stats.p99 * static_cast<uint64_t>(iterations) * wl.timed.size();
    }
);

} // anonymous namespace

int main() {
    hftu::run_benchmarks();
    return 0;
}
