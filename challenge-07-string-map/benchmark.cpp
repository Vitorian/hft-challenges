// Benchmark harness for Challenge 07: String Map
// Do NOT modify this file — it will be overwritten during certified runs.
//
// Every timed lookup is a copy of its key in a std::string of its own (never the object
// insert() received), and the lookups come in a random order. Every timed answer is
// checked. Validations: a small table, 10,000 keys in insertion order, and 100,000
// held-out keys whose copies plus 20,000 misses are looked up shuffled and then in
// insertion order. The certified run uses the same checks with other seeds.

#include "common/benchmark_harness.h"
#include "solution/solution.h"

#include <cstdlib>
#include <vector>
#include <string>
#include <set>
#include <unordered_set>

namespace {

std::vector<std::string> generate_unique_keys(size_t count, size_t max_len, uint64_t seed) {
    std::mt19937_64 gen(seed);
    std::uniform_int_distribution<int> len_dist(1, static_cast<int>(max_len));
    std::uniform_int_distribution<int> char_dist('A', 'z');

    std::set<std::string> seen;
    std::vector<std::string> keys;
    keys.reserve(count);
    while (keys.size() < count) {
        int len = len_dist(gen);
        std::string s(len, ' ');
        for (int j = 0; j < len; ++j) s[j] = static_cast<char>(char_dist(gen));
        if (seen.insert(s).second) keys.push_back(std::move(s));
    }
    return keys;
}

std::vector<std::string> generate_miss_keys(size_t count, uint64_t seed) {
    std::mt19937_64 gen(seed);
    std::vector<std::string> keys;
    keys.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        // Use digits to guarantee miss (keys are alpha only)
        int len = 1 + (gen() % 16);
        std::string s(len, ' ');
        for (int j = 0; j < len; ++j) s[j] = static_cast<char>('0' + (gen() % 10));
        keys.push_back(std::move(s));
    }
    return keys;
}

// Misses drawn like the keys (same lengths and alphabet), none of them in `keys`.
std::vector<std::string> generate_near_misses(const std::vector<std::string>& keys, size_t count,
                                              uint64_t seed) {
    std::unordered_set<std::string> known(keys.begin(), keys.end());
    std::mt19937_64 gen(seed);
    std::uniform_int_distribution<int> len_dist(1, 16);
    std::uniform_int_distribution<int> char_dist('A', 'z');
    std::vector<std::string> out;
    out.reserve(count);
    while (out.size() < count) {
        int len = len_dist(gen);
        std::string s(len, ' ');
        for (int j = 0; j < len; ++j) s[j] = static_cast<char>(char_dist(gen));
        if (!known.count(s)) out.push_back(std::move(s));
    }
    return out;
}

// A lookup: a copy of the key in its own std::string, and the value it must find
// (kMiss for none).
constexpr uint32_t kMiss = UINT32_MAX;
struct Query {
    std::string key;
    uint32_t expected;
};

// Copies of `hits` (expected value = index) and `misses`, in a random order.
std::vector<Query> make_queries(const std::vector<std::string>& hits,
                                const std::vector<std::string>& misses, uint64_t order_seed) {
    std::vector<Query> q;
    q.reserve(hits.size() + misses.size());
    for (size_t i = 0; i < hits.size(); ++i) q.push_back({std::string(hits[i]), static_cast<uint32_t>(i)});
    for (const auto& m : misses) q.push_back({std::string(m), kMiss});
    std::mt19937_64 gen(order_seed);
    std::shuffle(q.begin(), q.end(), gen);
    return q;
}

bool answer_ok(const uint32_t* v, uint32_t expected) {
    return expected == kMiss ? v == nullptr : (v != nullptr && *v == expected);
}

std::string describe(const std::string& key, const uint32_t* v, uint32_t expected) {
    std::string want = expected == kMiss ? "nullptr" : std::to_string(expected);
    std::string got = v ? std::to_string(*v) : "nullptr";
    return "find(\"" + key + "\") gave " + got + ", expected " + want;
}

// A wrong timed answer ends the run as a failed validation, whether the harness
// validates before or after it times.
void check_timed(const char* bench, const std::vector<Query>& q, const std::vector<const uint32_t*>& out) {
    for (size_t j = 0; j < q.size(); ++j) {
        if (!answer_ok(out[j], q[j].expected)) {
            std::string msg = std::string(bench) + ": " + describe(q[j].key, out[j], q[j].expected);
            hftu::check_failed("timed", msg.c_str());
            std::printf("{\"error\": \"Validation failed\", \"benchmarks\": []}\n");
            std::fflush(stdout);
            std::fflush(stderr);
            std::_Exit(0);
        }
    }
}

// Inserts keys[i] -> i, then times one find per query; returns the cycles.
uint64_t timed_run(const char* bench, const std::vector<std::string>& keys, const std::vector<Query>& q,
                   bool time_inserts) {
    std::vector<const uint32_t*> out(q.size());
    hftu::StringMap sm;
    uint64_t start = 0;
    if (time_inserts) start = hftu::cycle_start();
    for (size_t j = 0; j < keys.size(); ++j)
        sm.insert(keys[j].c_str(), keys[j].size(), static_cast<uint32_t>(j));
    if (!time_inserts) start = hftu::cycle_start();
    for (size_t j = 0; j < q.size(); ++j) {
        const uint32_t* v = sm.find(q[j].key.c_str(), q[j].key.size());
        out[j] = v;
        hftu::do_not_optimize(v);
    }
    hftu::clobber();
    uint64_t end = hftu::cycle_end();
    check_timed(bench, q, out);
    return end - start;
}

} // namespace

// Correctness validation
static hftu::RegisterValidation val_correctness("correctness", []() -> bool {
    bool pass = true;

    hftu::StringMap sm;
    sm.insert("AAPL", 4, 100);
    sm.insert("MSFT", 4, 200);
    sm.insert("GOOG", 4, 300);

    auto* v = sm.find("AAPL", 4);
    if (!v || *v != 100) { hftu::check_failed("correctness", "AAPL should be 100"); pass = false; }

    v = sm.find("MSFT", 4);
    if (!v || *v != 200) { hftu::check_failed("correctness", "MSFT should be 200"); pass = false; }

    v = sm.find("NOTHERE", 7);
    if (v) { hftu::check_failed("correctness", "NOTHERE should be nullptr"); pass = false; }

    // Larger test
    auto keys = generate_unique_keys(10'000, 16, 0xDCBA);
    hftu::StringMap sm2;
    for (size_t i = 0; i < keys.size(); ++i) {
        sm2.insert(keys[i].c_str(), keys[i].size(), static_cast<uint32_t>(i));
    }
    for (size_t i = 0; i < keys.size(); ++i) {
        auto* val = sm2.find(keys[i].c_str(), keys[i].size());
        if (!val || *val != static_cast<uint32_t>(i)) {
            char msg[128];
            std::snprintf(msg, sizeof(msg), "key %zu: expected %zu, got %s",
                          i, i, val ? std::to_string(*val).c_str() : "nullptr");
            hftu::check_failed("correctness", msg);
            pass = false;
            break;
        }
    }

    if (pass) std::fprintf(stderr, "  all correctness tests passed\n");
    return pass;
});

// 100,000 held-out keys; copies of them plus 20,000 misses, shuffled; then the keys again
// in insertion order, from copies, on the same map.
static hftu::RegisterValidation val_heldout("heldout", []() -> bool {
    auto keys = generate_unique_keys(100'000, 16, 0x5EED'0701ULL);
    auto misses = generate_miss_keys(10'000, 0x5EED'0702ULL);
    auto near = generate_near_misses(keys, 10'000, 0x5EED'0703ULL);
    misses.insert(misses.end(), near.begin(), near.end());
    auto q = make_queries(keys, misses, 0x5EED'0704ULL);

    hftu::StringMap sm;
    for (size_t i = 0; i < keys.size(); ++i)
        sm.insert(keys[i].c_str(), keys[i].size(), static_cast<uint32_t>(i));
    for (const auto& x : q) {
        const uint32_t* v = sm.find(x.key.c_str(), x.key.size());
        if (!answer_ok(v, x.expected)) return hftu::check_failed("heldout", describe(x.key, v, x.expected).c_str());
    }
    for (size_t i = 0; i < keys.size(); ++i) {
        std::string copy(keys[i]);
        const uint32_t* v = sm.find(copy.c_str(), copy.size());
        if (!answer_ok(v, static_cast<uint32_t>(i)))
            return hftu::check_failed("heldout", ("in insertion order: " + describe(copy, v, static_cast<uint32_t>(i))).c_str());
    }
    std::fprintf(stderr, "  heldout: %zu lookups verified (%zu keys, %zu misses)\n",
                 q.size() + keys.size(), keys.size(), misses.size());
    return true;
});

// Mixed insert + lookup
static hftu::RegisterBenchmark reg_solution(
    "BM_Solution", 200'000,
    [](int iterations) -> uint64_t {
        auto keys = generate_unique_keys(100'000, 16, 0xDEAD);
        auto q = make_queries(keys, {}, 0xDEAD'0001ULL);
        uint64_t total = 0;
        for (int i = 0; i < iterations; ++i) total += timed_run("BM_Solution", keys, q, true);
        return total;
    }
);

// Lookup-only (insert first, then time lookups)
static hftu::RegisterBenchmark reg_lookup(
    "BM_LookupOnly", 100'000,
    [](int iterations) -> uint64_t {
        auto keys = generate_unique_keys(100'000, 16, 0xBEEF);
        auto q = make_queries(keys, {}, 0xBEEF'0001ULL);
        uint64_t total = 0;
        for (int i = 0; i < iterations; ++i) total += timed_run("BM_LookupOnly", keys, q, false);
        return total;
    }
);

// Miss-heavy: 50% hits, 50% misses
static hftu::RegisterBenchmark reg_miss(
    "BM_MissHeavy", 100'000,
    [](int iterations) -> uint64_t {
        auto keys = generate_unique_keys(50'000, 16, 0x2222'1111);
        auto misses = generate_miss_keys(50'000, 0x4444'3333);
        auto q = make_queries(keys, misses, 0x6666);
        uint64_t total = 0;
        for (int i = 0; i < iterations; ++i) total += timed_run("BM_MissHeavy", keys, q, false);
        return total;
    }
);

int main() {
    hftu::run_benchmarks();
    return 0;
}
