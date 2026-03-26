#include "common/benchmark_harness.h"
#include "pipeline.h"
#include "types.h"

#include <random>
#include <vector>

namespace {

std::vector<Message> generate_messages(int count, uint64_t seed) {
    std::mt19937_64 rng(seed);
    std::vector<Message> msgs(count);
    std::uniform_real_distribution<double> price_dist(90.0, 110.0);
    std::uniform_real_distribution<double> qty_dist(1.0, 1000.0);
    std::uniform_int_distribution<uint32_t> sym_dist(0, 199);
    std::uniform_int_distribution<int> type_dist(0, 99);

    for (int i = 0; i < count; i++) {
        msgs[i].timestamp = static_cast<int64_t>(i) * 1000;
        msgs[i].price = price_dist(rng);
        msgs[i].quantity = qty_dist(rng);
        msgs[i].symbol_id = sym_dist(rng);
        int r = type_dist(rng);
        if (r < 45)      msgs[i].type = 0;
        else if (r < 75)  msgs[i].type = 1;
        else if (r < 90)  msgs[i].type = 2;
        else if (r < 97)  msgs[i].type = 3;
        else               msgs[i].type = 4;
        msgs[i].flags = (r >= 99) ? 0x02 : 0x00;
    }
    return msgs;
}

static auto messages = generate_messages(100000, 0xB01AD1);

static hftu::RegisterBenchmark reg_solution("BM_Solution", messages.size(),
    [](int iters) -> uint64_t {
        std::vector<Result> results(messages.size());
        uint64_t total = 0;

        for (int i = 0; i < iters; i++) {
            std::memset(results.data(), 0, results.size() * sizeof(Result));
            auto start = hftu::cycle_start();
            run_pipeline(messages.data(), messages.size(), results.data());
            hftu::clobber();
            total += hftu::cycle_end() - start;
        }
        return total;
    });

} // anon namespace

int main() {
    hftu::run_benchmarks();
    return 0;
}
