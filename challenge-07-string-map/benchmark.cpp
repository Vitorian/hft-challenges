// Benchmark harness for Challenge 07: String Map
// Do NOT modify this file — it will be overwritten during certified runs.

#include "common/benchmark_harness.h"
#include "solution/solution.h"

#include <vector>
#include <string>

namespace {

// Generate random strings up to max_len characters
std::vector<std::string> generate_keys(size_t count, size_t max_len, uint64_t seed) {
    std::mt19937_64 gen(seed);
    std::uniform_int_distribution<int> len_dist(1, static_cast<int>(max_len));
    std::uniform_int_distribution<int> char_dist('A', 'Z');

    std::vector<std::string> keys;
    keys.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        int len = len_dist(gen);
        std::string s(len, ' ');
        for (int j = 0; j < len; ++j) s[j] = static_cast<char>(char_dist(gen));
        keys.push_back(std::move(s));
    }
    return keys;
}

} // namespace

// Mixed insert + lookup
static hftu::RegisterBenchmark reg_solution(
    "BM_Solution", 200'000,
    [](int iterations) -> uint64_t {
        auto keys = generate_keys(100'000, 16, 0xDEAD);
        uint64_t total = 0;

        for (int i = 0; i < iterations; ++i) {
            hftu::StringMap sm;
            uint64_t start = hftu::cycle_start();
            // Insert all
            for (size_t j = 0; j < keys.size(); ++j) {
                sm.insert(keys[j].c_str(), keys[j].size(), static_cast<uint32_t>(j));
            }
            // Lookup all
            for (size_t j = 0; j < keys.size(); ++j) {
                auto* v = sm.find(keys[j].c_str(), keys[j].size());
                hftu::do_not_optimize(v);
            }
            hftu::clobber();
            uint64_t end = hftu::cycle_end();
            total += (end - start);
        }
        return total;
    }
);

int main() {
    hftu::run_benchmarks();
    return 0;
}
