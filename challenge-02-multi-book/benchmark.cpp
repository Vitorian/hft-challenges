// Benchmark harness for Challenge 02: Multi-Symbol Order Book
// Do NOT modify this file — it will be overwritten during certified runs.

#include "common/benchmark_harness.h"
#include "venue.h"
#include "solution/solution.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <queue>
#include <random>
#include <vector>
#include <unordered_map>
#include <unordered_set>

namespace {

// ---------------------------------------------------------------------------
// Venue implementation — deterministic exchange simulator
// ---------------------------------------------------------------------------

class VenueImpl : public hftu::Venue {
    uint64_t next_eid_;

    struct OurInfo { int64_t price; int64_t qty; };
    std::unordered_map<uint64_t, OurInfo> our_by_eid_;

public:
    explicit VenueImpl(uint64_t start_eid) : next_eid_(start_eid) {}

    void reset_next_eid(uint64_t eid) {
        next_eid_ = eid;
    }

    uint64_t peek_next_eid() const { return next_eid_; }

    uint64_t send_order(uint64_t /*our_id*/, uint16_t /*symbol*/, int /*side*/,
                        int64_t price, int64_t qty) override {
        uint64_t eid = next_eid_;
        our_by_eid_[eid] = {price, qty};
        return eid;
    }

    uint64_t modify_order(uint64_t exchange_id,
                          int64_t new_price, int64_t new_qty) override {
        auto it = our_by_eid_.find(exchange_id);
        if (it == our_by_eid_.end()) return exchange_id;

        if (new_price != it->second.price || new_qty > it->second.qty) {
            our_by_eid_.erase(it);
            uint64_t new_eid = next_eid_;
            our_by_eid_[new_eid] = {new_price, new_qty};
            return new_eid;
        }
        it->second.qty = new_qty;
        return exchange_id;
    }

    void cancel_order(uint64_t exchange_id) override {
        our_by_eid_.erase(exchange_id);
    }
};

// ---------------------------------------------------------------------------
// Operation types
// ---------------------------------------------------------------------------

struct Operation {
    enum Type : uint8_t {
        // Exchange feed
        ADD, CANCEL, MODIFY,
        // Our orders
        SEND_OUR, MODIFY_OUR, CANCEL_OUR,
        // Queries
        BEST_BID, BEST_ASK,
        TOP_LEVELS, VOLUME_NEAR_BEST,
        QUEUE_POSITION,
    };
    Type type;
    uint64_t id;          // exchange_id (feed) or our_id (our ops)
    uint16_t symbol;
    int8_t side;
    int64_t price;
    int64_t qty;
    int64_t aux;          // depth for VOLUME_NEAR_BEST, n for TOP_LEVELS
};

// ---------------------------------------------------------------------------
// Run a single operation on the book, return result for correctness checks
// ---------------------------------------------------------------------------

struct OpResult {
    hftu::TopLevel top{};
    hftu::QueuePosition qpos{};
    hftu::TopLevel levels[5]{};
    int num_levels = 0;
    int64_t volume = 0;
};

inline OpResult execute_op(hftu::MultiOrderBook& book, const Operation& op) {
    OpResult r;
    switch (op.type) {
        case Operation::ADD:
            book.add_order(op.id, op.symbol, op.side, op.price, op.qty);
            break;
        case Operation::CANCEL:
            book.cancel_order(op.id);
            break;
        case Operation::MODIFY:
            book.modify_order(op.id, op.qty);
            break;
        case Operation::SEND_OUR:
            book.send_order(op.id, op.symbol, op.side, op.price, op.qty);
            break;
        case Operation::MODIFY_OUR:
            book.modify_our_order(op.id, op.price, op.qty);
            break;
        case Operation::CANCEL_OUR:
            book.cancel_our_order(op.id);
            break;
        case Operation::BEST_BID:
            r.top = book.best_bid(op.symbol);
            hftu::do_not_optimize(r.top);
            break;
        case Operation::BEST_ASK:
            r.top = book.best_ask(op.symbol);
            hftu::do_not_optimize(r.top);
            break;
        case Operation::TOP_LEVELS:
            r.num_levels = book.get_top_levels(op.symbol, op.side,
                                               static_cast<int>(op.aux), r.levels);
            hftu::do_not_optimize(r.num_levels);
            break;
        case Operation::VOLUME_NEAR_BEST:
            r.volume = book.volume_near_best(op.symbol, op.side, op.aux);
            hftu::do_not_optimize(r.volume);
            break;
        case Operation::QUEUE_POSITION:
            r.qpos = book.get_queue_position(op.id);
            hftu::do_not_optimize(r.qpos);
            break;
    }
    return r;
}

// ---------------------------------------------------------------------------
// Workload generation
// ---------------------------------------------------------------------------

struct WorkloadConfig {
    int num_symbols = 200;
    int num_hot = 50;
    double hot_pct = 0.9;
    size_t setup_orders = 500'000;
    size_t timed_ops = 100'000;
    int max_our_orders = 50;
    int our_order_latency = 10;   // ops between send and corresponding ADD
    int64_t max_price = 100'000;
    uint64_t seed = 42;
};

struct GeneratedWorkload {
    std::vector<Operation> setup;
    std::vector<Operation> timed;
    uint64_t venue_start_eid;  // exchange_id the venue should start at after setup
};

// Symbol mid prices — spread across the price range
inline int64_t symbol_mid_price(int sym, int64_t max_price) {
    return 1000 + static_cast<int64_t>(sym) * (max_price - 2000) / 200;
}

inline uint16_t pick_symbol(std::mt19937_64& gen, int num_symbols, int num_hot, double hot_pct) {
    std::uniform_real_distribution<double> pct(0.0, 1.0);
    if (pct(gen) < hot_pct) {
        return static_cast<uint16_t>(std::uniform_int_distribution<int>(0, num_hot - 1)(gen));
    }
    return static_cast<uint16_t>(std::uniform_int_distribution<int>(num_hot, num_symbols - 1)(gen));
}

inline int64_t pick_price(std::mt19937_64& gen, uint16_t symbol, int64_t max_price) {
    int64_t mid = symbol_mid_price(symbol, max_price);
    std::uniform_real_distribution<double> pct(0.0, 1.0);
    if (pct(gen) < 0.8) {
        // Near mid: within 100 ticks
        int64_t offset = std::uniform_int_distribution<int64_t>(-100, 100)(gen);
        return std::max(int64_t(1), std::min(max_price, mid + offset));
    }
    // Far from mid
    return std::uniform_int_distribution<int64_t>(1, max_price)(gen);
}

GeneratedWorkload generate_workload(const WorkloadConfig& cfg) {
    std::mt19937_64 gen(cfg.seed);
    GeneratedWorkload wl;
    wl.setup.reserve(cfg.setup_orders);

    uint64_t next_eid = 1;
    uint64_t next_our_id = 1;

    constexpr bool our_order = true;
    constexpr bool not_our_order = false;
    // Track active exchange orders (for cancel/modify targets)
    struct ActiveOrder {
        uint64_t eid;
        uint16_t symbol;
        int8_t side;
        int64_t price;
        int64_t qty;
        bool is_our;
    };
    std::vector<ActiveOrder> active;
    active.reserve(cfg.setup_orders + cfg.timed_ops);

    // Track our active orders
    struct OurOrder {
        uint64_t our_id;
        uint64_t exchange_id;
        uint16_t symbol;
        int8_t side;
        int64_t price;
        int64_t qty;
        size_t min_trigger_at{};
    };
    std::vector<OurOrder> our_active;

    // Pending feed events from our order actions
    struct PendingEvent {
        size_t trigger_at;
        Operation op;
        bool operator>(const PendingEvent& o) const { return trigger_at > o.trigger_at; }
    };
    std::priority_queue<PendingEvent, std::vector<PendingEvent>, std::greater<>> pending;

    std::uniform_int_distribution<int64_t> qty_dist(1, 10'000);
    std::uniform_int_distribution<int> side_dist(0, 1);

    // --- Setup phase: populate book with orders ---
    for (size_t i = 0; i < cfg.setup_orders; ++i) {
        uint16_t sym = pick_symbol(gen, cfg.num_symbols, cfg.num_hot, cfg.hot_pct);
        int8_t side = static_cast<int8_t>(side_dist(gen));
        int64_t price = pick_price(gen, sym, cfg.max_price);
        int64_t qty = qty_dist(gen);
        uint64_t eid = next_eid++;

        wl.setup.push_back({Operation::ADD, eid, sym, side, price, qty, 0});
        active.push_back({eid, sym, side, price, qty, not_our_order});
    }
    wl.venue_start_eid = next_eid;

    // --- Timed phase ---
    wl.timed.reserve(cfg.timed_ops * 2); // extra room for pending events
    std::uniform_int_distribution<int> op_dist(0, 99);
    size_t op_idx = 0;

    auto drain_pending = [&]() {
        while (!pending.empty() && pending.top().trigger_at <= op_idx) {
            wl.timed.push_back(pending.top().op);
            op_idx++;
            pending.pop();
        }
    };

    for (size_t i = 0; i < cfg.timed_ops; ++i) {
        drain_pending();

        int r = op_dist(gen);

        if (r < 35 || active.size() < 100) {
            // ADD (exchange feed) — 35%
            uint16_t sym = pick_symbol(gen, cfg.num_symbols, cfg.num_hot, cfg.hot_pct);
            int8_t side = static_cast<int8_t>(side_dist(gen));
            int64_t price = pick_price(gen, sym, cfg.max_price);
            int64_t qty = qty_dist(gen);
            uint64_t eid = next_eid++;
            wl.timed.push_back({Operation::ADD, eid, sym, side, price, qty, 0});
            active.push_back({eid, sym, side, price, qty, not_our_order});

        } else if (r < 55 && active.size() > 1000) {
            // CANCEL (exchange feed) — 20%
            std::uniform_int_distribution<size_t> idx(0, active.size() - 1);
            size_t ci = idx(gen);

            for( auto j = 0U; j < active.size(); ++j ) {
                auto& ao = active[(ci + j)%active.size()];
                // Generate cancel events only for external orders
                // if our order is happen to be randomly selected
                // we will pick another one.
                if( !ao.is_our ) {
                    uint64_t eid = ao.eid;
                    ao = active.back();
                    active.pop_back();
                    wl.timed.push_back({Operation::CANCEL, eid, 0, 0, 0, 0, 0});
                    break;
                }
            }

        } else if (r < 65 && active.size() > 100) {
            // MODIFY (exchange feed, qty-down) — 10%
            std::uniform_int_distribution<size_t> idx(0, active.size() - 1);
            size_t mi = idx(gen);

            for( auto j = 0U; j < active.size(); ++j ) {
                auto& ao = active[(mi + j)%active.size()];
                // Generate modify events only for external orders
                // if our order is happen to be randomly selected
                // we will pick another one.
                if( !ao.is_our ) {
                    int64_t new_qty = std::max(int64_t(1), ao.qty * std::uniform_int_distribution<int64_t>(30, 90)(gen) / 100);
                    ao.qty = new_qty;
                    wl.timed.push_back({Operation::MODIFY, ao.eid, 0, 0, 0, new_qty, 0});
                    break;
                }
            }

        } else if (r < 70 && static_cast<int>(our_active.size()) < cfg.max_our_orders) {
            // SEND_OUR — 5%
            uint16_t sym = pick_symbol(gen, cfg.num_symbols, cfg.num_hot, cfg.hot_pct);
            int8_t side = static_cast<int8_t>(side_dist(gen));
            int64_t price = pick_price(gen, sym, cfg.max_price);
            int64_t qty = qty_dist(gen);
            uint64_t oid = next_our_id++;
            uint64_t eid = next_eid++; // venue will assign this

            wl.timed.push_back({Operation::SEND_OUR, oid, sym, side, price, qty, static_cast<int64_t>(eid)});

            // Schedule ADD in feed after latency
            int delay = std::uniform_int_distribution<int>(3, cfg.our_order_latency)(gen);
            auto trigger_at = op_idx + static_cast<size_t>(delay) + 1;
            pending.push({trigger_at, {Operation::ADD, eid, sym, side, price, qty, 0}});

            our_active.push_back({oid, eid, sym, side, price, qty, trigger_at + 1});
            active.push_back({eid, sym, side, price, qty, our_order});

        } else if (r < 73 && !our_active.empty()) {
            // MODIFY_OUR — 3%
            std::uniform_int_distribution<size_t> idx(0, our_active.size() - 1);
            size_t mi = idx(gen);
            auto& oo = our_active[mi];

            // 60% qty-down, 40% price-change
            if (std::uniform_int_distribution<int>(0, 99)(gen) < 60) {
                // Qty-down: keeps position
                int64_t new_qty = std::max(int64_t(1), oo.qty * std::uniform_int_distribution<int64_t>(30, 90)(gen) / 100);
                wl.timed.push_back({Operation::MODIFY_OUR, oo.our_id, 0, 0, oo.price, new_qty, static_cast<int64_t>(oo.exchange_id)});

                // Feed: modify with same eid
                int delay = std::uniform_int_distribution<int>(3, cfg.our_order_latency)(gen);
                size_t trigger_at = std::max<size_t>( op_idx + static_cast<size_t>(delay) + 1, oo.min_trigger_at);
                pending.push({trigger_at,
                              {Operation::MODIFY, oo.exchange_id, 0, 0, 0, new_qty, 0}});
                oo.qty = new_qty;
                oo.min_trigger_at = trigger_at + 1;
                // Update active list qty
                for (auto& a : active) {
                    if (a.eid == oo.exchange_id) { a.qty = new_qty; break; }
                }
            } else {
                // Price change: loses position — cancel + add
                int64_t new_price = pick_price(gen, oo.symbol, cfg.max_price);
                int64_t new_qty = oo.qty;
                uint64_t old_eid = oo.exchange_id;
                uint64_t new_eid = next_eid++; // venue will assign this

                wl.timed.push_back({Operation::MODIFY_OUR, oo.our_id, 0, 0, new_price, new_qty, static_cast<int64_t>(new_eid)});

                int delay = std::uniform_int_distribution<int>(3, cfg.our_order_latency)(gen);
                size_t trigger_at = std::max<size_t>( op_idx + static_cast<size_t>(delay) + 1, oo.min_trigger_at);

                pending.push({trigger_at, {Operation::CANCEL, old_eid, 0, 0, 0, 0, 0}});
                pending.push({trigger_at + 1, {Operation::ADD, new_eid, oo.symbol, oo.side, new_price, new_qty, static_cast<int64_t>(new_eid)}});
                oo.min_trigger_at = trigger_at + 2;

                // Update tracking
                for (auto& a : active) {
                    if (a.eid == old_eid) { a.eid = new_eid; a.price = new_price; break; }
                }
                oo.exchange_id = new_eid;
                oo.price = new_price;
            }

        } else if (r < 75 && !our_active.empty()) {
            // CANCEL_OUR — 2%
            std::uniform_int_distribution<size_t> idx(0, our_active.size() - 1);
            size_t ci = idx(gen);
            auto& oo = our_active[ci];

            wl.timed.push_back({Operation::CANCEL_OUR, oo.our_id, 0, 0, 0, 0, 0});

            int delay = std::uniform_int_distribution<int>(3, cfg.our_order_latency)(gen);
            size_t trigger_at = std::max<size_t>( op_idx + static_cast<size_t>(delay) + 1, oo.min_trigger_at);
            pending.push({trigger_at,
                          {Operation::CANCEL, oo.exchange_id, 0, 0, 0, 0, 0}});

            // Remove from active lists
            for (size_t j = 0; j < active.size(); ++j) {
                if (active[j].eid == oo.exchange_id) {
                    active[j] = active.back(); active.pop_back(); break;
                }
            }
            our_active[ci] = our_active.back();
            our_active.pop_back();

        } else if (r < 80) {
            // BEST_BID — 5%
            uint16_t sym = pick_symbol(gen, cfg.num_symbols, cfg.num_hot, cfg.hot_pct);
            wl.timed.push_back({Operation::BEST_BID, 0, sym, 0, 0, 0, 0});

        } else if (r < 85) {
            // BEST_ASK — 5%
            uint16_t sym = pick_symbol(gen, cfg.num_symbols, cfg.num_hot, cfg.hot_pct);
            wl.timed.push_back({Operation::BEST_ASK, 0, sym, 0, 0, 0, 0});

        } else if (r < 90) {
            // TOP_LEVELS — 5%
            uint16_t sym = pick_symbol(gen, cfg.num_symbols, cfg.num_hot, cfg.hot_pct);
            int8_t side = static_cast<int8_t>(side_dist(gen));
            int n = std::uniform_int_distribution<int>(1, 5)(gen);
            wl.timed.push_back({Operation::TOP_LEVELS, 0, sym, side, 0, 0, n});

        } else if (r < 95) {
            // VOLUME_NEAR_BEST — 5%
            uint16_t sym = pick_symbol(gen, cfg.num_symbols, cfg.num_hot, cfg.hot_pct);
            int8_t side = static_cast<int8_t>(side_dist(gen));
            int64_t depth = std::uniform_int_distribution<int64_t>(5, 50)(gen);
            wl.timed.push_back({Operation::VOLUME_NEAR_BEST, 0, sym, side, 0, 0, depth});

        } else {
            // QUEUE_POSITION — 5%
            if (!our_active.empty()) {
                std::uniform_int_distribution<size_t> idx(0, our_active.size() - 1);
                wl.timed.push_back({Operation::QUEUE_POSITION, our_active[idx(gen)].our_id,
                                    0, 0, 0, 0, 0});
            } else {
                // No our orders — do a best_bid instead
                uint16_t sym = pick_symbol(gen, cfg.num_symbols, cfg.num_hot, cfg.hot_pct);
                wl.timed.push_back({Operation::BEST_BID, 0, sym, 0, 0, 0, 0});
            }
        }

        op_idx = wl.timed.size();
    }

    // Drain remaining pending events
    while (!pending.empty()) {
        wl.timed.push_back(pending.top().op);
        pending.pop();
    }

    return wl;
}

// ---------------------------------------------------------------------------
// Latency percentile computation
// ---------------------------------------------------------------------------

struct LatencyStats {
    uint64_t p50;
    uint64_t p99;
    uint64_t p999;
    uint64_t max;
    double avg;
    size_t count;
};

LatencyStats compute_stats(std::vector<uint64_t>& latencies) {
    if (latencies.empty()) return {};
    std::sort(latencies.begin(), latencies.end());
    size_t n = latencies.size();
    uint64_t sum = 0;
    for (auto l : latencies) sum += l;
    return {
        latencies[n * 50 / 100],
        latencies[std::min(n - 1, n * 99 / 100)],
        latencies[std::min(n - 1, n * 999 / 1000)],
        latencies.back(),
        static_cast<double>(sum) / static_cast<double>(n),
        n,
    };
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Benchmarks
// ---------------------------------------------------------------------------

static hftu::RegisterBenchmark reg_solution(
    "BM_Solution", 100'000,
    [](int iterations) -> uint64_t {
        WorkloadConfig cfg;
        cfg.seed = 0x02'CAFE'BABE;

        const auto wl = generate_workload(cfg);

        std::vector<uint64_t> all_latencies;
        all_latencies.reserve(wl.timed.size() * static_cast<size_t>(iterations));

        uint64_t total_cycles = 0;

        for (int iter = 0; iter < iterations; ++iter) {
            VenueImpl venue(wl.venue_start_eid);
            hftu::MultiOrderBook book(venue);

            // Setup (not timed)
            for (const auto& op : wl.setup) {
                venue.reset_next_eid( op.aux );
                execute_op(book, op);
            }

            // Timed — per-operation measurement
            for (const auto& op : wl.timed) {
                venue.reset_next_eid( op.aux );
                uint64_t t0 = hftu::cycle_start();
                execute_op(book, op);
                hftu::clobber();
                uint64_t t1 = hftu::cycle_end();
                uint64_t lat = t1 - t0;
                all_latencies.push_back(lat);
                total_cycles += lat;
            }
        }

        // Print latency stats to stderr for user feedback
        auto stats = compute_stats(all_latencies);
        std::fprintf(stderr, "  Latency (cycles): p50=%lu  p99=%lu  p999=%lu  max=%lu  avg=%.0f\n",
                     stats.p50, stats.p99, stats.p999, stats.max, stats.avg);

        return total_cycles;
    }
);

int main() {
    hftu::run_benchmarks();
    return 0;
}
