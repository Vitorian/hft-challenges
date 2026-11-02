// Benchmark harness for Challenge 11: Rolling Counter (v2, 2026-11)
// Do NOT modify this file — it will be overwritten during certified runs.
//
// How the score is measured (identical to the certified benchmark, which only
// differs in its seeds):
//   * The event stream (time steps and event counts) is generated BEFORE the
//     timed region. Inside the loop there is nothing but your three calls, a
//     32-bit add and one store per op.
//   * Every count() is recorded and the recorded run is verified afterwards
//     against an exact oracle with the ± precision contract: per op, with
//     cutoff = now - interval, lo = events with time > cutoff + precision and
//     hi = events with time >= cutoff - precision; your count must satisfy
//     lo <= count() <= hi.
//   * The score is cycles per thousand ops (cycles/kop): median of ten passes
//     of one million ops each, after one untimed warm-up pass. Fresh instance
//     and fresh stream for every pass.
//   * Three goals: BM_Solution (1 s window, 1 ms precision, ~1 kHz),
//     BM_HighRate (1 ms / 1 us, 100-150 ns steps), BM_Sparse (1 s / 10 ms,
//     jumps up to 0.5 s). Plus the edge-case checks you can read below.

#include "common/benchmark_harness.h"
#include "common/hftu_xoshiro.h"
#include "solution/solution.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

// ---------------------------------------------------------------------------
// Seeds (PUBLIC — the certified benchmark uses different ones)
// ---------------------------------------------------------------------------
constexpr uint64_t SEED_STANDARD   = 0x000000000000ABCDULL;
constexpr uint64_t SEED_HIGH_RATE  = 0x000000000000FA57ULL;
constexpr uint64_t SEED_SPARSE     = 0x00000000005BAB5EULL;
constexpr uint64_t SEED_VALIDATION = 0x000000000C0AAEC7ULL;

constexpr int    SCORED_PASSES = 10;        // timed passes; the median counts
constexpr size_t SCORED_OPS    = 1'000'000; // per pass; ops_per_iteration = SCORED_OPS / 1000

struct Op { uint32_t dt; uint32_t ev; };    // 8 bytes/op: 1M ops = 8 MB

struct Workload {
    const char* name;
    int64_t interval, precision;
    uint32_t dt_min; uint64_t dt_span;      // dt = dt_min + rng.next_range(dt_span)
    uint32_t ev_min; uint64_t ev_span;      // ev = ev_min + rng.next_range(ev_span)
    uint64_t seed;
};

// Same three shapes as v1: ~1 kHz with a 1 s / 1 ms window; 100-150 ns steps
// with a 1 ms / 1 us window; sparse jumps up to 0.5 s with a 1 s / 10 ms window.
constexpr Workload W_STANDARD  = {"BM_Solution", 1'000'000'000LL, 1'000'000LL,  1'000'000u, 500'001,     0, 6,  SEED_STANDARD};
constexpr Workload W_HIGH_RATE = {"BM_HighRate", 1'000'000LL,     1'000LL,      100u,       51,          1, 20, SEED_HIGH_RATE};
constexpr Workload W_SPARSE    = {"BM_Sparse",   1'000'000'000LL, 10'000'000LL, 1'000'000u, 499'000'001, 0, 3,  SEED_SPARSE};

uint64_t pass_seed(uint64_t seed, int pass) {
    return seed ^ (static_cast<uint64_t>(pass + 1) * 0x9E3779B97F4A7C15ULL);
}

// Fills ops[] and returns the start time of the stream (a few minutes to
// hours into a day, never 0: the API only promises monotonic timestamps).
int64_t fill_stream(std::vector<Op>& ops, const Workload& w, uint64_t seed) {
    hftu::Rng rng(seed);
    const int64_t t0 = 1'000'000'000'000LL + static_cast<int64_t>(rng.next_range(1ULL << 40));
    for (auto& o : ops) {
        o.dt = w.dt_min + static_cast<uint32_t>(rng.next_range(w.dt_span));
        o.ev = w.ev_min + static_cast<uint32_t>(rng.next_range(w.ev_span));
    }
    return t0;
}

// ---------------------------------------------------------------------------
// Oracle: the exact event log with the ± precision contract.
// For the current time t and cutoff = t - interval:
//   lo = sum of events with time >  cutoff + precision   (certainly in the window)
//   hi = sum of events with time >= cutoff - precision   (possibly in the window)
// A correct counter reports lo <= count() <= hi. Timestamps are monotonic, so
// both cursors only move forward: O(1) amortised per op.
// ---------------------------------------------------------------------------
class Oracle {
    int64_t interval_, precision_, t_ = 0;
    std::vector<std::pair<int64_t, uint64_t>> ev_;
    size_t lo_i_ = 0, hi_i_ = 0;
    uint64_t sum_lo_ = 0, sum_hi_ = 0, total_ = 0;
public:
    Oracle(int64_t interval, int64_t precision) : interval_(interval), precision_(precision) {}
    void reserve(size_t n) { ev_.reserve(n); }
    void update(int64_t t) { t_ = t; }
    void addEvent(uint64_t c) { ev_.push_back({t_, c}); total_ += c; }
    void bounds(uint64_t& lo, uint64_t& hi) {
        const int64_t cutoff = t_ - interval_;
        while (hi_i_ < ev_.size() && ev_[hi_i_].first < cutoff - precision_) { sum_hi_ += ev_[hi_i_].second; ++hi_i_; }
        while (lo_i_ < ev_.size() && ev_[lo_i_].first <= cutoff + precision_) { sum_lo_ += ev_[lo_i_].second; ++lo_i_; }
        hi = total_ - sum_hi_;
        lo = total_ - sum_lo_;
    }
};

// ---------------------------------------------------------------------------
// The timed loop. Nothing but the solution's three calls, a 32-bit add and one
// store per op; the stream and the record live in L3.
// ---------------------------------------------------------------------------
uint64_t timed_pass(hftu::RollingCounter& rc, int64_t t, const Op* p, size_t n, uint32_t* rec) {
    const uint64_t start = hftu::cycle_start();
    for (size_t j = 0; j < n; ++j) {
        t += p[j].dt;
        rc.update(t);
        rc.addEvent(p[j].ev);
        rec[j] = static_cast<uint32_t>(rc.count());
    }
    hftu::clobber();
    const uint64_t end = hftu::cycle_end();
    hftu::do_not_optimize(t);
    return end - start;
}

// Replays the stream through the oracle and checks every recorded count.
// Returns the index of the first bad op, or n if all are within bounds.
size_t verify_recorded(const Workload& w, int64_t t, const std::vector<Op>& ops, const std::vector<uint32_t>& rec,
                       uint64_t& got, uint64_t& lo, uint64_t& hi) {
    Oracle o(w.interval, w.precision);
    o.reserve(ops.size());
    for (size_t j = 0; j < ops.size(); ++j) {
        t += ops[j].dt;
        o.update(t);
        o.addEvent(ops[j].ev);
        o.bounds(lo, hi);
        got = rec[j];
        if (got < lo || got > hi) return j;
    }
    return ops.size();
}

// Result of the scored run, read by the "scored_counts" validation. The
// certified harness runs benchmarks first and validations after; the public
// one validates first. ensure_scored() runs each workload exactly once,
// whichever side asks first, so this file works unchanged in both.
struct ScoredCheck {
    const char* name = "";
    bool ran = false, ok = true;
    int pass = -1; size_t op = 0; uint64_t got = 0, lo = 0, hi = 0;
    uint64_t median_cycles = 0;
};
ScoredCheck g_scored[3];

uint64_t median_of(std::vector<uint64_t> v) {
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

// One untimed warm-up pass on a throwaway instance, then SCORED_PASSES timed
// passes on fresh instances and fresh streams; every pass is verified; the
// median pass is the score (cycles for SCORED_OPS ops).
uint64_t run_scored(const Workload& w, ScoredCheck& check) {
    std::vector<Op> ops(SCORED_OPS);
    std::vector<uint32_t> rec(SCORED_OPS);
    check.name = w.name;
    check.ran = true;
    check.ok = true;
    {
        const int64_t t0 = fill_stream(ops, w, pass_seed(w.seed, -1));
        hftu::RollingCounter rc(w.interval, w.precision);
        timed_pass(rc, t0, ops.data(), ops.size(), rec.data());
        hftu::do_not_optimize(rec);
    }
    std::vector<uint64_t> cycles;
    cycles.reserve(SCORED_PASSES);
    for (int pass = 0; pass < SCORED_PASSES; ++pass) {
        const int64_t t0 = fill_stream(ops, w, pass_seed(w.seed, pass));
        hftu::RollingCounter rc(w.interval, w.precision);
        cycles.push_back(timed_pass(rc, t0, ops.data(), ops.size(), rec.data()));
        if (check.ok) {
            uint64_t got, lo, hi;
            const size_t bad = verify_recorded(w, t0, ops, rec, got, lo, hi);
            if (bad < ops.size()) { check.ok = false; check.pass = pass; check.op = bad; check.got = got; check.lo = lo; check.hi = hi; }
        }
    }
    check.median_cycles = median_of(cycles);
    return check.median_cycles;
}

const Workload* const WORKLOADS[3] = {&W_STANDARD, &W_HIGH_RATE, &W_SPARSE};

// Runs workload i once, the first time anyone asks for it.
uint64_t ensure_scored(int i) {
    if (!g_scored[i].ran) run_scored(*WORKLOADS[i], g_scored[i]);
    return g_scored[i].median_cycles;
}

// ---------------------------------------------------------------------------
// Validation helpers
// ---------------------------------------------------------------------------
bool fail(const char* test, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
bool fail(const char* test, const char* fmt, ...) {
    char msg[256];
    va_list ap; va_start(ap, fmt); std::vsnprintf(msg, sizeof msg, fmt, ap); va_end(ap);
    return hftu::check_failed(test, msg);
}

// Streams a workload shape (untimed) through the solution and the oracle,
// checking every op. `n` ops from `seed`.
bool stream_check(const char* test, const char* label, const Workload& w, size_t n, uint64_t seed) {
    std::vector<Op> ops(n);
    const int64_t t0 = fill_stream(ops, w, seed);
    hftu::RollingCounter rc(w.interval, w.precision);
    Oracle o(w.interval, w.precision);
    o.reserve(n);
    int64_t t = t0;
    for (size_t j = 0; j < n; ++j) {
        t += ops[j].dt;
        rc.update(t); o.update(t);
        rc.addEvent(ops[j].ev); o.addEvent(ops[j].ev);
        uint64_t lo, hi;
        o.bounds(lo, hi);
        const uint64_t got = rc.count();
        if (got < lo || got > hi)
            return fail(test, "%s: op %zu: count %llu outside [%llu, %llu]", label, j,
                        (unsigned long long)got, (unsigned long long)lo, (unsigned long long)hi);
    }
    return true;
}

bool expect_eq(const char* test, const char* label, uint64_t got, uint64_t want) {
    if (got == want) return true;
    return fail(test, "%s: count %llu, expected %llu", label, (unsigned long long)got, (unsigned long long)want);
}

bool expect_in(const char* test, const char* label, uint64_t got, uint64_t lo, uint64_t hi) {
    if (got >= lo && got <= hi) return true;
    return fail(test, "%s: count %llu outside [%llu, %llu]", label, (unsigned long long)got,
                (unsigned long long)lo, (unsigned long long)hi);
}

} // namespace

// ---------------------------------------------------------------------------
// Validations (registration order is the report order)
// ---------------------------------------------------------------------------

// The documented example, the size_t conversion, and the three workload shapes
// streamed through the oracle on seeds of their own.
static hftu::RegisterValidation val_correctness("correctness", []() -> bool {
    const char* T = "correctness";
    bool pass = true;
    {
        hftu::RollingCounter rc(1'000'000'000LL, 1'000'000LL);
        rc.update(100'000'000); rc.addEvent(5);
        rc.update(600'000'000); rc.addEvent(3);
        pass &= expect_eq(T, "example: both events in the window", rc.count(), 8);
        rc.update(1'200'000'000LL);
        pass &= expect_eq(T, "example: first event expired", rc.count(), 3);
        pass &= expect_eq(T, "operator size_t", static_cast<size_t>(rc), 3);
    }
    {
        hftu::RollingCounter rc(100, 10);
        rc.update(50); rc.addEvent(10);
        rc.update(200);
        pass &= expect_eq(T, "everything expired", rc.count(), 0);
    }
    const uint64_t s = SEED_VALIDATION;
    pass &= stream_check(T, "standard stream",  W_STANDARD,  200'000, s ^ 0x1111111111111111ULL);
    pass &= stream_check(T, "high-rate stream", W_HIGH_RATE, 200'000, s ^ 0x2222222222222222ULL);
    pass &= stream_check(T, "sparse stream",    W_SPARSE,    200'000, s ^ 0x3333333333333333ULL);
    if (pass) std::fprintf(stderr, "  correctness: example, size_t, three streams of 200k ops\n");
    return pass;
});

// Regimes the three workloads never exercise.
static hftu::RegisterValidation val_edge_cases("edge_cases", []() -> bool {
    const char* T = "edge_cases";
    bool pass = true;
    constexpr int64_t I = 1'000'000'000LL, P = 1'000'000LL;

    // count() before any update, addEvent before any update, zero-count events.
    // The API does not say what "the current time" is before the first update
    // (the skeleton uses 0; stamping them at the first update is as valid), so
    // the checks hold under either reading: still inside at I - 2P, gone at
    // 2I + 2P.
    {
        hftu::RollingCounter rc(I, P);
        pass &= expect_eq(T, "count before any update", rc.count(), 0);
        rc.addEvent(4);
        rc.addEvent(0);
        pass &= expect_eq(T, "events added before the first update", rc.count(), 4);
        rc.update(I - 2 * P);
        pass &= expect_eq(T, "events from before the first update, still inside", rc.count(), 4);
        rc.update(2 * I + 2 * P);
        pass &= expect_eq(T, "events from before the first update, expired", rc.count(), 0);
    }
    // repeated update() with the same timestamp; jumps far beyond the window
    {
        hftu::RollingCounter rc(I, P);
        rc.update(5 * I); rc.addEvent(7);
        rc.update(5 * I); rc.update(5 * I);
        pass &= expect_eq(T, "repeated update with the same timestamp", rc.count(), 7);
        rc.update(5 * I + I / 2); rc.addEvent(1);
        rc.update(50 * I);
        pass &= expect_eq(T, "jump of 45 windows", rc.count(), 0);
        rc.addEvent(2);
        pass &= expect_eq(T, "events after the jump", rc.count(), 2);
    }
    // jumps of exactly interval ± precision around a single event
    {
        hftu::RollingCounter rc(I, P);
        const int64_t e = 3 * I + 123'457;
        rc.update(e); rc.addEvent(9);
        rc.update(e + I - 2 * P);
        pass &= expect_eq(T, "event at cutoff + 2 precision", rc.count(), 9);
        rc.update(e + I);
        pass &= expect_in(T, "event exactly at the cutoff", rc.count(), 0, 9);
        rc.update(e + I + 2 * P);
        pass &= expect_eq(T, "event at cutoff - 2 precision", rc.count(), 0);
    }
    // totals past 2^32
    {
        hftu::RollingCounter rc(I, P);
        rc.update(7 * I);
        rc.addEvent(3'000'000'000ULL); rc.addEvent(3'000'000'000ULL);
        pass &= expect_eq(T, "total past 2^32", rc.count(), 6'000'000'000ULL);
        rc.update(7 * I + I / 2); rc.addEvent(1);
        rc.update(8 * I + 2 * P);
        pass &= expect_eq(T, "large events expired", rc.count(), 1);
    }
    // non-power-of-two parameters; interval not a multiple of precision
    {
        const Workload w = {"odd", 1'000'000'007LL, 999LL, 500u, 2'000, 0, 4, 0};
        pass &= stream_check(T, "interval 1,000,000,007 / precision 999", w, 100'000, SEED_VALIDATION ^ 0x4444444444444444ULL);
    }
    // precision 1 ns with a 1,000 ns window: exact semantics forced
    {
        const Workload w = {"exact", 1'000LL, 1LL, 1u, 40, 0, 3, 0};
        pass &= stream_check(T, "precision 1 ns, window 1,000 ns", w, 100'000, SEED_VALIDATION ^ 0x5555555555555555ULL);
    }
    // a deep window: interval / precision = 10^6
    {
        const Workload w = {"deep", 1'000'000'000LL, 1'000LL, 100u, 1'901, 0, 3, 0};
        pass &= stream_check(T, "window of 10^6 precision units", w, 200'000, SEED_VALIDATION ^ 0x6666666666666666ULL);
    }
    // start times near 2^62 and near INT64_MAX (20k steps of at most 1.5 ms
    // stay 20 windows short of the end)
    {
        for (int k = 0; k < 2; ++k) {
            const int64_t t0 = k == 0 ? (1LL << 62) : (INT64_MAX - 50 * I);
            hftu::RollingCounter rc(I, P);
            Oracle o(I, P);
            hftu::Rng rng(SEED_VALIDATION ^ (0x7777777777777777ULL + k));
            int64_t t = t0;
            bool ok = true;
            for (size_t j = 0; j < 20'000 && ok; ++j) {
                t += 1'000'000LL + static_cast<int64_t>(rng.next_range(500'001));
                const uint64_t ev = rng.next_range(6);
                rc.update(t); o.update(t);
                rc.addEvent(ev); o.addEvent(ev);
                uint64_t lo, hi; o.bounds(lo, hi);
                const uint64_t got = rc.count();
                if (got < lo || got > hi) {
                    ok = fail(T, "start near %s: op %zu: count %llu outside [%llu, %llu]",
                              k == 0 ? "2^62" : "INT64_MAX", j, (unsigned long long)got,
                              (unsigned long long)lo, (unsigned long long)hi);
                }
            }
            pass &= ok;
        }
    }
    // two counters alive and interleaved, different parameters
    {
        hftu::RollingCounter a(I, P), b(1'000'000LL, 1'000LL);
        Oracle oa(I, P), ob(1'000'000LL, 1'000LL);
        hftu::Rng rng(SEED_VALIDATION ^ 0x8888888888888888ULL);
        int64_t ta = 2 * I, tb = 2 * I;
        bool ok = true;
        for (size_t j = 0; j < 100'000 && ok; ++j) {
            ta += 1'000'000LL + static_cast<int64_t>(rng.next_range(500'001));
            tb += 100 + static_cast<int64_t>(rng.next_range(51));
            const uint64_t ea = rng.next_range(6), eb = 1 + rng.next_range(20);
            a.update(ta); oa.update(ta); a.addEvent(ea); oa.addEvent(ea);
            b.update(tb); ob.update(tb); b.addEvent(eb); ob.addEvent(eb);
            uint64_t lo, hi;
            oa.bounds(lo, hi);
            if (a.count() < lo || a.count() > hi) ok = fail(T, "two counters: counter A op %zu off", j);
            ob.bounds(lo, hi);
            if (ok && (b.count() < lo || b.count() > hi)) ok = fail(T, "two counters: counter B op %zu off", j);
        }
        pass &= ok;
    }
    // construct and destroy many instances
    {
        uint64_t sum = 0;
        for (int k = 0; k < 2'000; ++k) {
            hftu::RollingCounter rc(I, P);
            rc.update(I + k); rc.addEvent(1);
            sum += rc.count();
        }
        pass &= expect_eq(T, "2000 instances constructed and destroyed", sum, 2'000);
    }
    if (pass) std::fprintf(stderr, "  edge cases: all passed\n");
    return pass;
});

// The scored runs themselves: every recorded count() of every timed pass.
static hftu::RegisterValidation val_scored("scored_counts", []() -> bool {
    bool pass = true;
    for (int i = 0; i < 3; ++i) ensure_scored(i);
    for (const auto& c : g_scored) {
        if (!c.ok)
            pass = fail("scored_counts", "%s: pass %d op %zu: count %llu outside [%llu, %llu]", c.name, c.pass, c.op,
                        (unsigned long long)c.got, (unsigned long long)c.lo, (unsigned long long)c.hi);
    }
    if (pass) std::fprintf(stderr, "  scored counts: %d passes x %zu ops verified for 3 workloads\n", SCORED_PASSES, SCORED_OPS);
    return pass;
});

// ---------------------------------------------------------------------------
// Benchmarks: ops_per_iteration = SCORED_OPS / 1000, so cycles_per_op is
// cycles per thousand ops (unit cycles/kop). The harness calls each once;
// the ten passes and the median happen inside.
// ---------------------------------------------------------------------------
static hftu::RegisterBenchmark reg_solution(W_STANDARD.name, SCORED_OPS / 1000,
    [](int) -> uint64_t { return ensure_scored(0); }, 1);

static hftu::RegisterBenchmark reg_high_rate(W_HIGH_RATE.name, SCORED_OPS / 1000,
    [](int) -> uint64_t { return ensure_scored(1); }, 1);

static hftu::RegisterBenchmark reg_sparse(W_SPARSE.name, SCORED_OPS / 1000,
    [](int) -> uint64_t { return ensure_scored(2); }, 1);

int main() {
    return hftu::run_benchmarks();
}
