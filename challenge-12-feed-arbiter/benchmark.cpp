#include "solution/solution.h"
#include "../common/benchmark_harness.h"
#include <random>
#include <vector>

namespace {

hftu::Message make_message(uint64_t seq, std::mt19937_64& rng) {
    hftu::Message m{};
    m.sequence = seq;
    m.msg_type = 'A' + (rng() % 10);
    m.symbol_id = rng() % 5000;
    m.side = rng() % 2;
    m.price = 1000 + (rng() % 10000);
    m.quantity = 1 + (rng() % 1000);
    m.timestamp = seq * 1000;
    m.order_id = rng() % 1000000;
    return m;
}

struct FeedData {
    std::vector<hftu::Message> input;
    uint64_t max_sequence;
};

FeedData generate_feed(uint64_t num_messages, double gap_burst_prob,
                       int max_burst, uint64_t seed) {
    std::mt19937_64 rng(seed);
    FeedData fd;
    fd.max_sequence = num_messages;

    std::vector<hftu::Message> canonical(num_messages);
    for (uint64_t i = 0; i < num_messages; i++)
        canonical[i] = make_message(i + 1, rng);

    std::vector<hftu::Message> feed_a, feed_b;
    std::uniform_real_distribution<double> prob(0.0, 1.0);
    std::uniform_int_distribution<int> burst_dist(1, max_burst);

    uint64_t i = 0;
    while (i < num_messages) {
        if (prob(rng) < gap_burst_prob) {
            // Drop a burst from feed A
            int burst_len = std::min((uint64_t)burst_dist(rng), num_messages - i);
            for (int b = 0; b < burst_len; b++)
                feed_b.push_back(canonical[i + b]);
            i += burst_len;
        } else if (prob(rng) < gap_burst_prob) {
            // Drop a burst from feed B
            int burst_len = std::min((uint64_t)burst_dist(rng), num_messages - i);
            for (int b = 0; b < burst_len; b++)
                feed_a.push_back(canonical[i + b]);
            i += burst_len;
        } else {
            // Both feeds get it
            feed_a.push_back(canonical[i]);
            feed_b.push_back(canonical[i]);
            i++;
        }
    }

    // Interleave A and B
    size_t ai = 0, bi = 0;
    while (ai < feed_a.size() || bi < feed_b.size()) {
        bool pick_a;
        if (ai >= feed_a.size()) pick_a = false;
        else if (bi >= feed_b.size()) pick_a = true;
        else if (feed_a[ai].sequence <= feed_b[bi].sequence) pick_a = (prob(rng) < 0.6);
        else pick_a = (prob(rng) < 0.4);

        if (pick_a) fd.input.push_back(feed_a[ai++]);
        else fd.input.push_back(feed_b[bi++]);
    }

    return fd;
}

// Pre-generate feeds
static auto feed_small = generate_feed(50000, 0.10, 5, 0xFEED1);
static auto feed_burst = generate_feed(50000, 0.15, 50, 0xFEED2);
static auto feed_large = generate_feed(50000, 0.20, 500, 0xFEED3);

static hftu::RegisterBenchmark reg_small("small_gaps", feed_small.input.size(),
    [](int iters) -> uint64_t {
        auto& fd = feed_small;
        static std::vector<hftu::Message> out(fd.max_sequence + 1000);
        static hftu::FeedArbiter arbiter;
        uint64_t total = 0;

        for (int i = 0; i < iters; i++) {
            arbiter.build(fd.max_sequence);
            auto start = hftu::cycle_start();
            for (auto& msg : fd.input)
                arbiter.on_message(msg, out.data());
            hftu::clobber();
            total += hftu::cycle_end() - start;
        }
        return total;
    });

static hftu::RegisterBenchmark reg_burst("burst_gaps", feed_burst.input.size(),
    [](int iters) -> uint64_t {
        auto& fd = feed_burst;
        static std::vector<hftu::Message> out(fd.max_sequence + 1000);
        static hftu::FeedArbiter arbiter;
        uint64_t total = 0;

        for (int i = 0; i < iters; i++) {
            arbiter.build(fd.max_sequence);
            auto start = hftu::cycle_start();
            for (auto& msg : fd.input)
                arbiter.on_message(msg, out.data());
            hftu::clobber();
            total += hftu::cycle_end() - start;
        }
        return total;
    });

static hftu::RegisterBenchmark reg_large("large_gaps", feed_large.input.size(),
    [](int iters) -> uint64_t {
        auto& fd = feed_large;
        static std::vector<hftu::Message> out(fd.max_sequence + 1000);
        static hftu::FeedArbiter arbiter;
        uint64_t total = 0;

        for (int i = 0; i < iters; i++) {
            arbiter.build(fd.max_sequence);
            auto start = hftu::cycle_start();
            for (auto& msg : fd.input)
                arbiter.on_message(msg, out.data());
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
