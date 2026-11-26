#include "solution/solution.h"
#include "../common/benchmark_harness.h"
#include <random>
#include <vector>
#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstring>

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

// ---- Reference implementation for validation ----
struct RefBook {
    int num_legs;
    int depth;
    int weights[8];
    hftu::Level books[8][2][10];
    int book_sizes[8][2];

    void init(std::span<const int> w, int d) {
        num_legs = w.size(); depth = d;
        for (int i = 0; i < num_legs; i++) weights[i] = w[i];
        std::memset(books, 0, sizeof(books));
        std::memset(book_sizes, 0, sizeof(book_sizes));
    }

    void apply(const hftu::BookUpdate& u) {
        int leg = u.leg_index, side = u.side;
        int& sz = book_sizes[leg][side];
        int64_t price = u.level.price;
        if (u.action == 0 && sz < 10) {
            // Insert sorted
            int pos = 0;
            if (side == 0) { while (pos < sz && books[leg][side][pos].price > price) pos++; }
            else           { while (pos < sz && books[leg][side][pos].price < price) pos++; }
            for (int i = sz; i > pos; i--) books[leg][side][i] = books[leg][side][i-1];
            books[leg][side][pos] = u.level; sz++;
        } else if (u.action == 1) {
            for (int i = 0; i < sz; i++)
                if (books[leg][side][i].price == price) { books[leg][side][i].quantity = u.level.quantity; break; }
        } else if (u.action == 2) {
            for (int i = 0; i < sz; i++)
                if (books[leg][side][i].price == price) {
                    for (int j = i; j < sz - 1; j++) books[leg][side][j] = books[leg][side][j+1];
                    sz--; break;
                }
        }
    }

    int sweep(bool is_bid, hftu::Level* out) {
        int sides[8];
        for (int i = 0; i < num_legs; i++) {
            int w = weights[i];
            if (is_bid) sides[i] = (w > 0) ? 0 : 1;  // bid: sell w>0 (bid), buy w<0 (ask)
            else        sides[i] = (w > 0) ? 1 : 0;  // ask: buy w>0 (ask), sell w<0 (bid)
        }
        for (int i = 0; i < num_legs; i++)
            if (book_sizes[i][sides[i]] == 0) return 0;

        int64_t remaining[8][10];
        int pos[8];
        for (int i = 0; i < num_legs; i++) {
            pos[i] = 0;
            int s = sides[i];
            for (int j = 0; j < book_sizes[i][s]; j++)
                remaining[i][j] = books[i][s][j].quantity;
        }

        int count = 0;
        while (count < depth) {
            bool valid = true;
            for (int i = 0; i < num_legs; i++)
                if (pos[i] >= book_sizes[i][sides[i]]) { valid = false; break; }
            if (!valid) break;

            int64_t price = 0;
            for (int i = 0; i < num_legs; i++)
                price += weights[i] * books[i][sides[i]][pos[i]].price;

            int64_t min_qty = INT64_MAX;
            for (int i = 0; i < num_legs; i++) {
                int64_t avail = remaining[i][pos[i]] / std::abs(weights[i]);
                if (avail < min_qty) min_qty = avail;
            }
            if (min_qty <= 0) break;

            out[count++] = {price, min_qty};

            for (int i = 0; i < num_legs; i++) {
                remaining[i][pos[i]] -= min_qty * std::abs(weights[i]);
                if (remaining[i][pos[i]] <= 0) pos[i]++;
            }
        }
        return count;
    }

    void compute(hftu::Level* out_bids, int& nbids, hftu::Level* out_asks, int& nasks) {
        nbids = sweep(true, out_bids);
        nasks = sweep(false, out_asks);
    }
};

bool validate_on(hftu::ImpliedBook& book, const UpdateStream& us, const char* label) {
    book.build(us.weights, us.depth);
    RefBook ref;
    ref.init(us.weights, us.depth);

    hftu::Level bids[10], asks[10], ref_bids[10], ref_asks[10];
    int errors = 0;

    for (size_t i = 0; i < us.updates.size(); i++) {
        auto [nb, na] = book.on_update(us.updates[i], bids, asks);
        ref.apply(us.updates[i]);
        int rnb = 0, rna = 0;
        ref.compute(ref_bids, rnb, ref_asks, rna);

        if (nb != rnb || na != rna) {
            if (errors++ < 3)
                std::fprintf(stderr, "[%s] update %zu: level count mismatch bid %d!=%d ask %d!=%d\n",
                             label, i, nb, rnb, na, rna);
        }
        for (int j = 0; j < std::min(nb, rnb); j++) {
            if (bids[j].price != ref_bids[j].price || bids[j].quantity != ref_bids[j].quantity) {
                if (errors++ < 3)
                    std::fprintf(stderr, "[%s] update %zu: bid[%d] got {%ld,%ld} want {%ld,%ld}\n",
                                 label, i, j, bids[j].price, bids[j].quantity,
                                 ref_bids[j].price, ref_bids[j].quantity);
            }
        }
        for (int j = 0; j < std::min(na, rna); j++) {
            if (asks[j].price != ref_asks[j].price || asks[j].quantity != ref_asks[j].quantity) {
                if (errors++ < 3)
                    std::fprintf(stderr, "[%s] update %zu: ask[%d] got {%ld,%ld} want {%ld,%ld}\n",
                                 label, i, j, asks[j].price, asks[j].quantity,
                                 ref_asks[j].price, ref_asks[j].quantity);
            }
        }
    }

    if (errors > 0) {
        std::fprintf(stderr, "[%s] FAIL: %d errors in %zu updates\n", label, errors, us.updates.size());
        return false;
    }
    std::fprintf(stderr, "[%s] OK: %zu updates verified\n", label, us.updates.size());
    return true;
}

bool validate(const UpdateStream& us, const char* label) {
    hftu::ImpliedBook book;
    return validate_on(book, us, label);
}

// Pre-generate workloads (public seeds; the certified benchmark uses its own
// seeds, 100,000 updates per stream and held-out streams of its own)
static const int weights_butterfly[] = {1, -2, 1};
static const int weights_spread[] = {1, -1};
static const int weights_strip[] = {1, 1, 1};

static auto stream_butterfly = generate_updates(weights_butterfly, 5, 50000, 0xBF1A01);
static auto stream_spread = generate_updates(weights_spread, 5, 50000, 0x5BEAD1);
static auto stream_strip = generate_updates(weights_strip, 5, 50000, 0x57F1B0);

hftu::RegisterValidation val_butterfly("validate_butterfly", []() {
    return validate(stream_butterfly, "butterfly");
});
hftu::RegisterValidation val_spread("validate_spread", []() {
    return validate(stream_spread, "spread");
});
hftu::RegisterValidation val_strip("validate_strip", []() {
    return validate(stream_strip, "strip");
});

// Held-out streams (since 2026-11-26): the same generator and the same three
// workloads on seeds the timed run never sees, so an implementation tuned to
// the scored streams fails here. Each runs on one book object that is then
// rebuilt and fed the scored stream again, as the timed loop reuses its book
// across build() calls.
hftu::RegisterValidation val_heldout("validate_heldout", []() {
    const auto h_butterfly = generate_updates(weights_butterfly, 5, 50000, 0xea5eb30a357ee9baULL);
    const auto h_spread    = generate_updates(weights_spread,    5, 50000, 0x0b59cbe3410495d0ULL);
    const auto h_strip     = generate_updates(weights_strip,     5, 50000, 0x289486c91fff7bc2ULL);
    bool ok = true;
    {
        hftu::ImpliedBook book;
        ok &= validate_on(book, h_butterfly, "heldout butterfly");
        ok &= validate_on(book, stream_butterfly, "butterfly after rebuild");
    }
    {
        hftu::ImpliedBook book;
        ok &= validate_on(book, h_spread, "heldout spread");
        ok &= validate_on(book, stream_spread, "spread after rebuild");
    }
    {
        hftu::ImpliedBook book;
        ok &= validate_on(book, h_strip, "heldout strip");
        ok &= validate_on(book, stream_strip, "strip after rebuild");
    }
    return ok;
});

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
