// Benchmark harness for Challenge 10: FIX Builder
// Do NOT modify this file — it will be overwritten during certified runs.

#include "common/benchmark_harness.h"
#include "solution/solution.h"

#include <span>
#include <string>
#include <vector>

namespace {

static constexpr char SOH = '\x01';

// Build a realistic template for a NewOrderSingle
std::vector<std::pair<int, std::string>> make_template() {
    return {
        {8,   "FIX.4.4"},
        {35,  "D"},
        {49,  "ALGO-ENG1"},
        {56,  "NASDAQ-GW"},
        {1,   "HEDGE-MAIN"},
        {21,  "1"},
        {55,  "AAPL"},
        {40,  "2"},
        {59,  "0"},
        {15,  "USD"},
        {207, "XNAS"},
        {100, "XNAS"},
        {47,  "A"},
        {167, "CS"},
        {22,  "8"},
        {48,  "100123"},
        {58,  "Algo order from strategy MOMENTUM-v3.2 session 20260320"},
    };
}

// Generate a sequence of order fields
std::vector<hftu::OrderFields> generate_orders(size_t count, uint64_t seed) {
    std::mt19937_64 gen(seed);
    std::uniform_int_distribution<int64_t> price_dist(100'000'000LL, 999'999'999'999LL); // $1 to $9999.99
    std::uniform_int_distribution<int64_t> qty_dist(1, 50000);
    std::uniform_int_distribution<int> side_dist(1, 2);

    // Base timestamp: 2026-03-20 14:30:00 UTC in nanoseconds
    int64_t base_ts = 1774029000'000'000'000LL;

    std::vector<hftu::OrderFields> orders;
    orders.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        orders.push_back({
            .timestamp = base_ts + static_cast<int64_t>(i) * 1'000'000LL, // +1ms each
            .seq_num = static_cast<uint64_t>(i + 1),
            .cl_ord_id = 1'000'000ULL + i,
            .price = price_dist(gen),
            .quantity = qty_dist(gen),
            .side = static_cast<int8_t>(side_dist(gen)),
        });
    }
    return orders;
}

// Verify a FIX message is well-formed
bool verify_message(std::string_view msg, const hftu::OrderFields& order) {
    // Must start with 8=FIX.4.4\x01
    if (msg.find("8=FIX.4.4\x01") != 0) return false;
    // Must end with 10=xxx\x01
    if (msg.size() < 8 || msg.back() != SOH) return false;
    auto tag10 = msg.rfind("10=");
    if (tag10 == std::string_view::npos) return false;

    // Verify checksum
    int sum = 0;
    for (size_t i = 0; i < tag10; ++i) sum += static_cast<unsigned char>(msg[i]);
    sum &= 0xFF;
    auto cs_str = msg.substr(tag10 + 3, msg.size() - tag10 - 4);
    int cs = 0;
    for (char c : cs_str) if (c >= '0' && c <= '9') cs = cs * 10 + (c - '0');
    if (sum != cs) return false;

    // Verify body length
    auto tag9_pos = msg.find("9=");
    if (tag9_pos == std::string_view::npos) return false;
    auto tag9_end = msg.find(SOH, tag9_pos);
    auto body_len_str = msg.substr(tag9_pos + 2, tag9_end - tag9_pos - 2);
    int body_len = 0;
    for (char c : body_len_str) if (c >= '0' && c <= '9') body_len = body_len * 10 + (c - '0');
    size_t body_start = tag9_end + 1;
    size_t body_actual = tag10 - body_start;
    if (static_cast<size_t>(body_len) != body_actual) return false;

    return true;
}

} // namespace

static hftu::RegisterBenchmark reg_solution(
    "BM_Solution", 100'000,
    [](int iterations) -> uint64_t {
        auto tmpl = make_template();
        auto orders = generate_orders(100'000, 0xA1B2C3);

        uint64_t total = 0;
        for (int i = 0; i < iterations; ++i) {
            hftu::FixBuilder builder;
            builder.build(tmpl);

            uint64_t start = hftu::cycle_start();
            for (const auto& order : orders) {
                auto msg = builder.encode(order);
                hftu::do_not_optimize(msg);
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
