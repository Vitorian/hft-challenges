// Benchmark harness for Challenge 05: Ring Buffer (SPSC)
// Do NOT modify this file — it will be overwritten during certified runs.
//
// Open-loop latency. The producer sends on a fixed schedule and every message
// is timed from its SCHEDULED send time to the moment the consumer has it.
// Time the producer spends inside push(), or waiting before it can return,
// delays the messages behind it and shows up in their latency, exactly as it
// would for a feed handler that cannot get back to the network in time.
//
//   BM_Solution  feed: bursts of random size (1..64, mean ~8) at random gaps
//                (mean ~5 us, ~1.6M msg/s on average), every message of a
//                burst scheduled at the same instant. Score = mean latency
//                (cycles), median over rounds.
//   BM_Tail      p99 latency of the same feed rounds.
//   BM_Steady    one message every 200 ns (5M msg/s). Score = p50 latency.
//
// The consumer checks every message it pops (order and every field) against
// what was pushed; a mismatch fails the run. The certified run uses the same
// design with a different seed, more rounds, and pinned isolated cores.

#include "common/benchmark_harness.h"
#include "solution/solution.h"

#include <thread>
#include <atomic>
#include <climits>
#include <cmath>
#include <cstring>
#include <vector>
#include <algorithm>
#include <time.h>

namespace {

constexpr int PRODUCER_CPU_INDEX = 0;
constexpr int CONSUMER_CPU_INDEX = 1;
constexpr int ROUNDS = 5;
constexpr size_t FEED_OPS = 400'000;
constexpr size_t STEADY_OPS = 400'000;
constexpr uint64_t FEED_SEED = 0x0bb1ec0ffee05eedULL;   // the certified run uses a different seed
constexpr int MAX_BURST = 64;
constexpr double BURST_CONTINUE = 7.0 / 8.0;          // geometric burst size, mean ~8
constexpr double FEED_MIN_GAP_NS = 200.0;
constexpr double FEED_MEAN_EXTRA_GAP_NS = 4800.0;     // exponential, on top of the minimum
constexpr double STEADY_INTERVAL_NS = 200.0;

#if defined(__x86_64__) || defined(_M_X64)
inline uint64_t rdtsc_raw() {
    uint32_t lo, hi;
    asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return (static_cast<uint64_t>(hi) << 32) | lo;
}

// LFENCE keeps the read from running ahead of the pop that delivered the message.
inline uint64_t rdtsc_fenced() {
    uint32_t lo, hi;
    asm volatile("lfence\n\trdtsc" : "=a"(lo), "=d"(hi) :: "memory");
    return (static_cast<uint64_t>(hi) << 32) | lo;
}
#elif defined(__aarch64__)
inline uint64_t rdtsc_raw() {
    uint64_t v;
    asm volatile("mrs %0, cntvct_el0" : "=r"(v));
    return v;
}
inline uint64_t rdtsc_fenced() {
    uint64_t v;
    asm volatile("isb\n\tmrs %0, cntvct_el0" : "=r"(v) :: "memory");
    return v;
}
#else
inline uint64_t rdtsc_raw() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1'000'000'000ULL + ts.tv_nsec;
}
inline uint64_t rdtsc_fenced() { return rdtsc_raw(); }
#endif

double tsc_per_ns() {
    static const double ghz = [] {
        timespec a, b;
        clock_gettime(CLOCK_MONOTONIC, &a);
        uint64_t t0 = rdtsc_raw();
        double ns;
        do {
            clock_gettime(CLOCK_MONOTONIC, &b);
            ns = (b.tv_sec - a.tv_sec) * 1e9 + (b.tv_nsec - a.tv_nsec);
        } while (ns < 50e6);
        return (rdtsc_raw() - t0) / ns;
    }();
    return ghz;
}

struct SplitMix {
    uint64_t s;
    uint64_t next() {
        uint64_t z = (s += 0x9e3779b97f4a7c15ULL);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
        return z ^ (z >> 31);
    }
    double uniform() { return (next() >> 11) * (1.0 / 9007199254740992.0); }  // [0,1)
};

// Scheduled send time of message i, in TSC cycles relative to the round start.
std::vector<uint64_t> make_feed_schedule(size_t n, uint64_t seed) {
    std::vector<uint64_t> sched(n);
    SplitMix rng{seed};
    const double cyc = tsc_per_ns();
    double t = 0;
    size_t i = 0;
    while (i < n) {
        int burst = 1;
        while (burst < MAX_BURST && rng.uniform() < BURST_CONTINUE) ++burst;
        for (int k = 0; k < burst && i < n; ++k) sched[i++] = static_cast<uint64_t>(t);
        double gap_ns = FEED_MIN_GAP_NS - FEED_MEAN_EXTRA_GAP_NS * std::log(1.0 - rng.uniform());
        t += gap_ns * cyc;
    }
    return sched;
}

std::vector<uint64_t> make_steady_schedule(size_t n) {
    std::vector<uint64_t> sched(n);
    const double step = STEADY_INTERVAL_NS * tsc_per_ns();
    for (size_t i = 0; i < n; ++i) sched[i] = static_cast<uint64_t>(i * step);
    return sched;
}

// Latency histogram: 1-cycle bins up to ~42 us at 3.1 GHz, then overflow.
// The mean uses the exact sum, overflow included.
struct LatencyHistogram {
    static constexpr size_t NUM_BUCKETS = 1 << 17;
    std::vector<uint64_t> buckets = std::vector<uint64_t>(NUM_BUCKETS);
    uint64_t count = 0;
    uint64_t sum = 0;

    void record(uint64_t latency) {
        buckets[latency < NUM_BUCKETS ? latency : NUM_BUCKETS - 1]++;
        count++;
        sum += latency;
    }
    void reset() {
        std::fill(buckets.begin(), buckets.end(), 0);
        count = 0;
        sum = 0;
    }
    uint64_t percentile(double pct) const {
        uint64_t target = static_cast<uint64_t>(count * pct);
        uint64_t cumulative = 0;
        for (size_t i = 0; i < NUM_BUCKETS; i++) {
            cumulative += buckets[i];
            if (cumulative >= target) return i;
        }
        return NUM_BUCKETS;
    }
    uint64_t mean() const { return count ? sum / count : 0; }
};

struct RoundResult {
    uint64_t mean, p50, p99;
};

inline void fill_message(hftu::Message& m, size_t i, uint64_t ts) {
    m.timestamp = ts;
    m.sequence = i;
    m.symbol_id = static_cast<uint32_t>(i & 0xFFF);
    m.side = static_cast<uint16_t>(i & 1);
    m.flags = static_cast<uint16_t>((i >> 3) & 0xFF);
    m.price = static_cast<int64_t>(i * 100 + 1);
    m.quantity = static_cast<int64_t>((i & 0xFF) + 1);
    m.order_id = static_cast<int64_t>(i ^ 0x5A5A5A);
}

inline bool message_ok(const hftu::Message& m, size_t i, uint64_t ts) {
    return m.timestamp == ts && m.sequence == i &&
           m.symbol_id == static_cast<uint32_t>(i & 0xFFF) &&
           m.side == static_cast<uint16_t>(i & 1) &&
           m.flags == static_cast<uint16_t>((i >> 3) & 0xFF) &&
           m.price == static_cast<int64_t>(i * 100 + 1) &&
           m.quantity == static_cast<int64_t>((i & 0xFF) + 1) &&
           m.order_id == static_cast<int64_t>(i ^ 0x5A5A5A);
}

// Shared state between the producer (main thread) and the consumer thread.
hftu::RingBuffer* g_rb = nullptr;
const std::vector<uint64_t>* g_sched = nullptr;
uint64_t g_start = 0;
std::atomic<int> g_round_signal{-1};
std::atomic<int> g_round_done{-1};
std::atomic<bool> g_shutdown{false};
std::atomic<uint64_t> g_bad_messages{0};
LatencyHistogram g_hist;

void consumer_thread_fn() {
    hftu::pin_to_isolated(CONSUMER_CPU_INDEX);
    int last_round = -1;
    for (;;) {
        int r;
        while ((r = g_round_signal.load(std::memory_order_acquire)) == last_round) {
            if (g_shutdown.load(std::memory_order_relaxed)) return;
        }
        if (g_shutdown.load(std::memory_order_relaxed)) return;
        last_round = r;

        const uint64_t* sched = g_sched->data();
        const size_t n_ops = g_sched->size();
        const uint64_t start = g_start;
        uint64_t bad = 0;
        hftu::Message msg{};
        for (size_t n = 0; n < n_ops;) {
            if (g_rb->pop(msg)) {
                uint64_t now = rdtsc_fenced();
                uint64_t due = start + sched[n];
                if (!message_ok(msg, n, due)) ++bad;
                g_hist.record(now > due ? now - due : 0);
                ++n;
            }
        }
        g_bad_messages.fetch_add(bad, std::memory_order_relaxed);
        g_round_done.store(r, std::memory_order_release);
    }
}

RoundResult run_round(const std::vector<uint64_t>& sched, int round_id) {
    g_hist.reset();
    g_sched = &sched;
    g_start = rdtsc_raw() + 100'000;   // ~30 us for the consumer to pick up the round
    g_round_signal.store(round_id, std::memory_order_release);

    hftu::Message msg{};
    const uint64_t start = g_start;
    for (size_t i = 0; i < sched.size(); ++i) {
        const uint64_t due = start + sched[i];
        while (rdtsc_raw() < due) {}
        fill_message(msg, i, due);
        while (!g_rb->push(msg)) {}
    }
    while (g_round_done.load(std::memory_order_acquire) != round_id) {}
    return {g_hist.mean(), g_hist.percentile(0.50), g_hist.percentile(0.99)};
}

uint64_t median_of(std::vector<uint64_t> v) {
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

// One warm-up round, then ROUNDS measured rounds on a fresh ring.
std::vector<RoundResult> run_workload(const std::vector<uint64_t>& sched) {
    hftu::pin_to_isolated(PRODUCER_CPU_INDEX);
    hftu::RingBuffer rb(1024);
    g_rb = &rb;
    g_round_signal.store(-1);
    g_round_done.store(-1);
    g_shutdown.store(false);
    std::thread consumer(consumer_thread_fn);

    std::vector<RoundResult> results;
    run_round(sched, 0);
    for (int i = 1; i <= ROUNDS; ++i) results.push_back(run_round(sched, i));

    g_shutdown.store(true, std::memory_order_release);
    g_round_signal.store(1 << 30, std::memory_order_release);
    consumer.join();
    g_rb = nullptr;
    return results;
}

uint64_t g_feed_p99 = 0;

} // namespace

static void fail_if_corrupted() {
    if (g_bad_messages.load() != 0) {
        std::fprintf(stderr, "FAIL [timed_integrity]: a message popped during the benchmark "
                             "did not match the one pushed\n");
        std::printf("{\"error\": \"Validation failed\", \"benchmarks\": []}\n");
        std::exit(1);
    }
}

// BM_Solution must stay first: it is the score.
static hftu::RegisterBenchmark reg_feed(
    "BM_Solution", 1,
    [](int) -> uint64_t {
        auto rounds = run_workload(make_feed_schedule(FEED_OPS, FEED_SEED));
        fail_if_corrupted();
        std::vector<uint64_t> means, p99s;
        for (auto& r : rounds) { means.push_back(r.mean); p99s.push_back(r.p99); }
        g_feed_p99 = median_of(p99s);
        return median_of(means);
    },
    1
);

// Same feed rounds as BM_Solution; reports their tail.
static hftu::RegisterBenchmark reg_tail(
    "BM_Tail", 1,
    [](int) -> uint64_t { return g_feed_p99; },
    1
);

static hftu::RegisterBenchmark reg_steady(
    "BM_Steady", 1,
    [](int) -> uint64_t {
        auto rounds = run_workload(make_steady_schedule(STEADY_OPS));
        fail_if_corrupted();
        std::vector<uint64_t> p50s;
        for (auto& r : rounds) p50s.push_back(r.p50);
        return median_of(p50s);
    },
    1
);

int main() {
    hftu::run_benchmarks();
    return 0;
}
