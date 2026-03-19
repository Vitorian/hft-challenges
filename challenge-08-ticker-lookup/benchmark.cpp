// Benchmark harness for Challenge 08: Ticker Lookup
// Do NOT modify this file — it will be overwritten during certified runs.

#include "common/benchmark_harness.h"
#include "solution/solution.h"

#include <vector>
#include <string>
#include <fstream>

namespace {

// Load tickers from file, or generate synthetic ones if file not found.
std::vector<std::string> load_tickers() {
    std::vector<std::string> tickers;

    // Try loading from file (available in both local and certified builds)
    for (const char* path : {"tickers.txt", "../tickers.txt"}) {
        std::ifstream f(path);
        if (f.is_open()) {
            std::string line;
            while (std::getline(f, line)) {
                if (!line.empty() && line.back() == '\r') line.pop_back();
                if (!line.empty()) tickers.push_back(line);
            }
            break;
        }
    }

    // Fallback: generate synthetic tickers
    if (tickers.empty()) {
        std::mt19937_64 gen(42);
        std::uniform_int_distribution<int> len_dist(1, 6);
        std::uniform_int_distribution<int> char_dist('A', 'Z');
        for (int i = 0; i < 7000; ++i) {
            int len = len_dist(gen);
            std::string s(len, ' ');
            for (int j = 0; j < len; ++j) s[j] = static_cast<char>(char_dist(gen));
            tickers.push_back(std::move(s));
        }
    }

    return tickers;
}

// Generate miss keys — random strings NOT in the ticker set
std::vector<std::string> generate_miss_keys(size_t count, uint64_t seed) {
    std::mt19937_64 gen(seed);
    std::uniform_int_distribution<int> len_dist(1, 6);
    std::uniform_int_distribution<int> char_dist('A', 'Z');
    std::vector<std::string> keys;
    keys.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        // Use digits to guarantee miss (tickers are A-Z only)
        int len = len_dist(gen);
        std::string s(len, ' ');
        for (int j = 0; j < len; ++j) s[j] = static_cast<char>('0' + (char_dist(gen) % 10));
        keys.push_back(std::move(s));
    }
    return keys;
}

} // namespace

// Mixed hits and misses (80/20)
static hftu::RegisterBenchmark reg_solution(
    "BM_Solution", 1'000'000,
    [](int iterations) -> uint64_t {
        auto tickers = load_tickers();
        auto misses = generate_miss_keys(tickers.size(), 0xBEEF);

        // Build entries
        std::vector<hftu::TickerEntry> entries;
        entries.reserve(tickers.size());
        for (size_t i = 0; i < tickers.size(); ++i) {
            entries.push_back({tickers[i].c_str(), tickers[i].size(), static_cast<uint32_t>(i)});
        }

        // Build lookup sequence: 80% hits, 20% misses
        std::mt19937_64 gen(0x1234);
        std::uniform_int_distribution<int> hit_dist(0, 99);
        std::uniform_int_distribution<size_t> idx_dist(0, tickers.size() - 1);
        std::uniform_int_distribution<size_t> miss_idx_dist(0, misses.size() - 1);

        struct Query { const char* key; size_t len; };
        std::vector<Query> queries(1'000'000);
        for (auto& q : queries) {
            if (hit_dist(gen) < 80) {
                size_t idx = idx_dist(gen);
                q = {tickers[idx].c_str(), tickers[idx].size()};
            } else {
                size_t idx = miss_idx_dist(gen);
                q = {misses[idx].c_str(), misses[idx].size()};
            }
        }

        uint64_t total = 0;
        for (int i = 0; i < iterations; ++i) {
            hftu::TickerLookup tl;
            tl.build(entries.data(), entries.size());

            uint64_t start = hftu::cycle_start();
            for (const auto& q : queries) {
                auto* v = tl.find(q.key, q.len);
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
