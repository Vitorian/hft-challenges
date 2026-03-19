// Benchmark harness for Challenge 08: Ticker Lookup
// Do NOT modify this file — it will be overwritten during certified runs.
//
// Public benchmark uses a sample ticker list. Certified runs use the full NASDAQ list
// with lookups weighted by market cap.

#include "common/benchmark_harness.h"
#include "solution/solution.h"

#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <numeric>

namespace {

struct TickerWeight {
    std::string symbol;
    uint64_t weight;
};

std::vector<TickerWeight> load_tickers() {
    std::vector<TickerWeight> tickers;

    for (const char* path : {"tickers.txt", "../tickers.txt"}) {
        std::ifstream f(path);
        if (f.is_open()) {
            std::string line;
            while (std::getline(f, line)) {
                if (line.empty()) continue;
                if (line.back() == '\r') line.pop_back();
                std::istringstream ss(line);
                std::string sym;
                uint64_t w = 1;
                ss >> sym >> w;
                if (!sym.empty()) tickers.push_back({sym, w > 0 ? w : 1});
            }
            break;
        }
    }

    // Fallback: synthetic
    if (tickers.empty()) {
        std::mt19937_64 gen(42);
        std::uniform_int_distribution<int> len_dist(1, 6);
        std::uniform_int_distribution<int> char_dist('A', 'Z');
        for (int i = 0; i < 7000; ++i) {
            int len = len_dist(gen);
            std::string s(len, ' ');
            for (int j = 0; j < len; ++j) s[j] = static_cast<char>(char_dist(gen));
            tickers.push_back({std::move(s), 1});
        }
    }
    return tickers;
}

// Generate miss keys (digits, guaranteed not in ticker set)
std::vector<std::string> generate_miss_keys(size_t count, uint64_t seed) {
    std::mt19937_64 gen(seed);
    std::vector<std::string> keys;
    keys.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        int len = 1 + (gen() % 6);
        std::string s(len, ' ');
        for (int j = 0; j < len; ++j) s[j] = static_cast<char>('0' + (gen() % 10));
        keys.push_back(std::move(s));
    }
    return keys;
}

// Build a weighted lookup sequence using alias method for O(1) sampling
std::vector<size_t> build_weighted_indices(const std::vector<TickerWeight>& tickers,
                                           size_t count, uint64_t seed) {
    std::mt19937_64 gen(seed);
    size_t n = tickers.size();

    // Compute cumulative weights (scaled to avoid precision issues)
    std::vector<double> weights(n);
    double total = 0;
    for (size_t i = 0; i < n; ++i) {
        weights[i] = static_cast<double>(tickers[i].weight);
        total += weights[i];
    }

    // Use std::discrete_distribution for weighted sampling
    std::discrete_distribution<size_t> dist(weights.begin(), weights.end());

    std::vector<size_t> indices(count);
    for (size_t i = 0; i < count; ++i) {
        indices[i] = dist(gen);
    }
    return indices;
}

} // namespace

// Mixed hits (80%) and misses (20%), weighted by market cap
static hftu::RegisterBenchmark reg_solution(
    "BM_Solution", 1'000'000,
    [](int iterations) -> uint64_t {
        auto tickers = load_tickers();
        auto misses = generate_miss_keys(tickers.size(), 0xBEEF);

        std::vector<hftu::TickerEntry> entries;
        entries.reserve(tickers.size());
        for (size_t i = 0; i < tickers.size(); ++i)
            entries.push_back({tickers[i].symbol.c_str(), tickers[i].symbol.size(),
                               static_cast<uint32_t>(i)});

        // Build weighted lookup sequence
        auto hit_indices = build_weighted_indices(tickers, 800'000, 0xFEED);
        std::mt19937_64 gen(0x1234);
        std::uniform_int_distribution<size_t> miss_dist(0, misses.size() - 1);

        struct Query { const char* key; size_t len; };
        std::vector<Query> queries;
        queries.reserve(1'000'000);
        size_t hit_idx = 0;
        for (size_t i = 0; i < 1'000'000; ++i) {
            if (i % 5 != 0 && hit_idx < hit_indices.size()) {
                // 80% hits, weighted
                size_t idx = hit_indices[hit_idx++];
                queries.push_back({tickers[idx].symbol.c_str(), tickers[idx].symbol.size()});
            } else {
                // 20% misses
                size_t idx = miss_dist(gen);
                queries.push_back({misses[idx].c_str(), misses[idx].size()});
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
