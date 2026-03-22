#include "solution/solution.h"
#include "../common/benchmark_harness.h"
#include <random>
#include <vector>

namespace {

struct UpdateStream {
    std::vector<int> weights;
    int depth;
    std::vector<hftu::BookUpdate> updates;
};

// Generate a realistic stream of book updates for a multi-leg synthetic
UpdateStream generate_updates(std::span<const int> weights, int depth,
                               int num_updates, uint64_t seed) {
    std::mt19937_64 rng(seed);
    UpdateStream us;
    us.weights.assign(weights.begin(), weights.end());
    us.depth = depth;

    int num_legs = weights.size();

    struct BookState {
        int sizes[2] = {0, 0};
        int64_t prices[2][10] = {};
        int64_t qtys[2][10] = {};
    };
    std::vector<BookState> state(num_legs);

    std::uniform_int_distribution<int> leg_dist(0, num_legs - 1);
    std::uniform_int_distribution<int> side_dist(0, 1);
    std::uniform_int_distribution<int64_t> qty_dist(1, 500);
    std::uniform_real_distribution<double> action_prob(0.0, 1.0);

    // Seed each leg with `depth` levels per side
    for (int leg = 0; leg < num_legs; leg++) {
        int64_t bid_base = 10000 - leg * 100;
        int64_t ask_base = 10050 - leg * 100;
        for (int i = 0; i < depth; i++) {
            int64_t bp = bid_base - i * 10;
            int64_t bq = qty_dist(rng);
            state[leg].prices[0][i] = bp;
            state[leg].qtys[0][i] = bq;
            state[leg].sizes[0]++;
            us.updates.push_back({(uint16_t)leg, 0, 0, {bp, bq}});

            int64_t ap = ask_base + i * 10;
            int64_t aq = qty_dist(rng);
            state[leg].prices[1][i] = ap;
            state[leg].qtys[1][i] = aq;
            state[leg].sizes[1]++;
            us.updates.push_back({(uint16_t)leg, 1, 0, {ap, aq}});
        }
    }

    // Generate random updates
    for (int u = 0; u < num_updates; u++) {
        int leg = leg_dist(rng);
        int side = side_dist(rng);
        int sz = state[leg].sizes[side];

        double p = action_prob(rng);
        hftu::BookUpdate upd{};
        upd.leg_index = leg;
        upd.side = side;

        if (sz == 0 || p < 0.4) {
            // Add a level
            if (sz < depth) {
                // Generate a new price that doesn't exist yet
                int64_t price;
                if (side == 0) // bid: pick a price below worst bid
                    price = state[leg].prices[0][sz - 1] - 10;
                else // ask: pick a price above worst ask
                    price = state[leg].prices[1][sz - 1] + 10;

                upd.action = 0;
                upd.level = {price, qty_dist(rng)};

                // Insert sorted into state
                int pos = sz; // append at end (worst level)
                state[leg].prices[side][pos] = price;
                state[leg].qtys[side][pos] = upd.level.quantity;
                state[leg].sizes[side]++;
            } else {
                // Book full, modify instead
                int pos = std::uniform_int_distribution<int>(0, sz - 1)(rng);
                upd.action = 1;
                upd.level = {state[leg].prices[side][pos], qty_dist(rng)};
                state[leg].qtys[side][pos] = upd.level.quantity;
            }
        } else if (p < 0.7) {
            // Modify
            if (sz > 0) {
                int pos = std::uniform_int_distribution<int>(0, sz - 1)(rng);
                upd.action = 1;
                upd.level = {state[leg].prices[side][pos], qty_dist(rng)};
                state[leg].qtys[side][pos] = upd.level.quantity;
            } else continue;
        } else {
            // Delete
            if (sz > 1) {
                int pos = std::uniform_int_distribution<int>(0, sz - 1)(rng);
                upd.action = 2;
                upd.level = {state[leg].prices[side][pos], state[leg].qtys[side][pos]};

                for (int i = pos; i < sz - 1; i++) {
                    state[leg].prices[side][i] = state[leg].prices[side][i+1];
                    state[leg].qtys[side][i] = state[leg].qtys[side][i+1];
                }
                state[leg].sizes[side]--;
            } else continue;
        }

        us.updates.push_back(upd);
    }

    return us;
}

// Pre-generate workloads
static const int weights_butterfly[] = {1, -2, 1};
static const int weights_spread[] = {1, -1};
static const int weights_strip[] = {1, 1, 1};

static auto stream_butterfly = generate_updates(weights_butterfly, 5, 50000, 0xBF1A01);
static auto stream_spread = generate_updates(weights_spread, 5, 50000, 0x5BEAD1);
static auto stream_strip = generate_updates(weights_strip, 5, 50000, 0x57F1B0);

static hftu::RegisterBenchmark reg_butterfly("butterfly_3leg", stream_butterfly.updates.size(),
    [](int iters) -> uint64_t {
        auto& us = stream_butterfly;
        static hftu::Level bids[10], asks[10];
        static hftu::ImpliedBook book;
        uint64_t total = 0;

        for (int i = 0; i < iters; i++) {
            book.build(us.weights, us.depth);
            auto start = hftu::cycle_start();
            for (auto& upd : us.updates)
                book.on_update(upd, bids, asks);
            hftu::clobber();
            total += hftu::cycle_end() - start;
        }
        return total;
    });

static hftu::RegisterBenchmark reg_spread("spread_2leg", stream_spread.updates.size(),
    [](int iters) -> uint64_t {
        auto& us = stream_spread;
        static hftu::Level bids[10], asks[10];
        static hftu::ImpliedBook book;
        uint64_t total = 0;

        for (int i = 0; i < iters; i++) {
            book.build(us.weights, us.depth);
            auto start = hftu::cycle_start();
            for (auto& upd : us.updates)
                book.on_update(upd, bids, asks);
            hftu::clobber();
            total += hftu::cycle_end() - start;
        }
        return total;
    });

static hftu::RegisterBenchmark reg_strip("strip_3leg", stream_strip.updates.size(),
    [](int iters) -> uint64_t {
        auto& us = stream_strip;
        static hftu::Level bids[10], asks[10];
        static hftu::ImpliedBook book;
        uint64_t total = 0;

        for (int i = 0; i < iters; i++) {
            book.build(us.weights, us.depth);
            auto start = hftu::cycle_start();
            for (auto& upd : us.updates)
                book.on_update(upd, bids, asks);
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
