// Benchmark harness for Challenge 09: FIX Parser
// Do NOT modify this file — it will be overwritten during certified runs.

#include "common/benchmark_harness.h"
#include "solution/solution.h"

#include <span>
#include <string>
#include <vector>
#include <cstdio>

namespace {

// --- FIX message generator ---

static constexpr char SOH = '\x01';

const char* MSG_TYPES[] = {"D", "8", "F", "G"};
const char* SIDES[] = {"1", "2"};
const char* TIFS[] = {"0", "1", "2", "3", "4"};
const char* ORD_TYPES[] = {"1", "2", "3", "4"};
const char* CURRENCIES[] = {"USD", "EUR", "GBP", "JPY", "CHF"};
const char* EXCHANGES[] = {"XNAS", "XNYS", "ARCX", "BATS", "EDGA", "IEXG"};
const char* ACCOUNTS[] = {"ACCT001", "ACCT002", "ACCT003", "HEDGE-MAIN", "PROP-ALPHA", "CLIENT-7792"};

const char* SYMBOLS[] = {
    "AAPL", "MSFT", "GOOGL", "AMZN", "TSLA", "META", "NVDA", "AVGO",
    "JPM", "V", "JNJ", "WMT", "XOM", "LLY", "MA", "COST",
    "ORCL", "MU", "ASML", "TSM", "NFLX", "ADBE", "CRM", "AMD",
};
constexpr size_t NUM_SYMBOLS = sizeof(SYMBOLS) / sizeof(SYMBOLS[0]);

// Generate a random text field of given length
std::string random_text(std::mt19937_64& gen, int len) {
    static const char chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 -_.";
    std::string s(len, ' ');
    std::uniform_int_distribution<int> dist(0, sizeof(chars) - 2);
    for (int i = 0; i < len; ++i) s[i] = chars[dist(gen)];
    return s;
}

// Compute FIX checksum (sum of all bytes mod 256)
std::string fix_checksum(const std::string& body) {
    int sum = 0;
    for (unsigned char c : body) sum += c;
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%03d", sum & 0xFF);
    return buf;
}

// Generate one FIX message
std::string generate_fix_message(std::mt19937_64& gen, int seq_num, bool bad_checksum) {
    std::uniform_int_distribution<int> msg_type_dist(0, 3);
    std::uniform_int_distribution<int> side_dist(0, 1);
    std::uniform_int_distribution<int> sym_dist(0, NUM_SYMBOLS - 1);
    std::uniform_int_distribution<int> price_whole(1, 9999);
    std::uniform_int_distribution<int> price_frac(0, 99);
    std::uniform_int_distribution<int> qty_dist(1, 50000);
    std::uniform_int_distribution<int> tif_dist(0, 4);
    std::uniform_int_distribution<int> ord_type_dist(0, 3);
    std::uniform_int_distribution<int> curr_dist(0, 4);
    std::uniform_int_distribution<int> exch_dist(0, 5);
    std::uniform_int_distribution<int> acct_dist(0, 5);
    std::uniform_int_distribution<int> text_len_dist(20, 120);

    const char* sym = SYMBOLS[sym_dist(gen)];
    int pw = price_whole(gen);
    int pf = price_frac(gen);

    // Build body (everything between BeginString and CheckSum)
    std::string body;
    body.reserve(512);

    // Tag 35 - MsgType
    body += "35="; body += MSG_TYPES[msg_type_dist(gen)]; body += SOH;
    // Tag 49 - SenderCompID
    body += "49=SENDER"; body += char('0' + (seq_num % 10)); body += "1"; body += SOH;
    // Tag 56 - TargetCompID
    body += "56=TARGET"; body += char('0' + (seq_num % 5)); body += "1"; body += SOH;
    // Tag 34 - MsgSeqNum
    body += "34="; body += std::to_string(seq_num); body += SOH;
    // Tag 52 - SendingTime
    body += "52=20260320-14:30:"; body += std::to_string(seq_num % 60); body += ".";
    body += std::to_string(100 + seq_num % 900); body += SOH;
    // Tag 11 - ClOrdID
    body += "11=ORD-"; body += std::to_string(100000 + seq_num); body += SOH;
    // Tag 1 - Account
    body += "1="; body += ACCOUNTS[acct_dist(gen)]; body += SOH;
    // Tag 55 - Symbol
    body += "55="; body += sym; body += SOH;
    // Tag 54 - Side
    body += "54="; body += SIDES[side_dist(gen)]; body += SOH;
    // Tag 44 - Price
    char price_buf[32];
    std::snprintf(price_buf, sizeof(price_buf), "%d.%02d", pw, pf);
    body += "44="; body += price_buf; body += SOH;
    // Tag 38 - OrderQty
    body += "38="; body += std::to_string(qty_dist(gen)); body += SOH;
    // Tag 40 - OrdType
    body += "40="; body += ORD_TYPES[ord_type_dist(gen)]; body += SOH;
    // Tag 59 - TimeInForce
    body += "59="; body += TIFS[tif_dist(gen)]; body += SOH;
    // Tag 60 - TransactTime
    body += "60=20260320-14:30:"; body += std::to_string(seq_num % 60); body += ".";
    body += std::to_string(100 + seq_num % 900); body += SOH;
    // Tag 21 - HandlInst
    body += "21=1"; body += SOH;
    // Tag 47 - OrderCapacity
    body += "47=A"; body += SOH;
    // Tag 15 - Currency
    body += "15="; body += CURRENCIES[curr_dist(gen)]; body += SOH;
    // Tag 207 - SecurityExchange
    body += "207="; body += EXCHANGES[exch_dist(gen)]; body += SOH;
    // Tag 100 - ExDestination
    body += "100="; body += EXCHANGES[exch_dist(gen)]; body += SOH;
    // Tag 167 - SecurityType
    body += "167=CS"; body += SOH;
    // Tag 58 - Text (variable-length filler)
    body += "58="; body += random_text(gen, text_len_dist(gen)); body += SOH;

    // Build full message: 8=FIX.4.4|9=<len>|<body>10=<checksum>|
    std::string header;
    header += "8=FIX.4.4"; header += SOH;
    header += "9="; header += std::to_string(body.size()); header += SOH;

    std::string pre_checksum = header + body;
    std::string cs = fix_checksum(pre_checksum);
    if (bad_checksum) {
        // Corrupt the checksum
        int bad_val = (std::stoi(cs) + 1) % 256;
        char buf[8];
        std::snprintf(buf, sizeof(buf), "%03d", bad_val);
        cs = buf;
    }

    return pre_checksum + "10=" + cs + SOH;
}

// Generate a batch of messages
struct Batch {
    std::string data;
    size_t message_count;
    size_t valid_count;
};

Batch generate_batch(size_t count, uint64_t seed, double bad_checksum_rate = 0.05) {
    std::mt19937_64 gen(seed);
    std::uniform_real_distribution<double> bad_dist(0.0, 1.0);

    Batch batch;
    batch.message_count = count;
    batch.valid_count = 0;
    batch.data.reserve(count * 350); // ~350 bytes per message average

    for (size_t i = 0; i < count; ++i) {
        bool bad = bad_dist(gen) < bad_checksum_rate;
        batch.data += generate_fix_message(gen, static_cast<int>(i + 1), bad);
        if (!bad) ++batch.valid_count;
    }
    return batch;
}

// Build ticker entries for the symbol set
std::vector<hftu::TickerEntry> build_entries() {
    static std::vector<std::string> sym_storage;
    sym_storage.clear();
    for (size_t i = 0; i < NUM_SYMBOLS; ++i) {
        sym_storage.emplace_back(SYMBOLS[i]);
    }
    std::vector<hftu::TickerEntry> entries;
    for (size_t i = 0; i < sym_storage.size(); ++i) {
        entries.push_back({sym_storage[i], static_cast<uint32_t>(i)});
    }
    return entries;
}

} // namespace

// Mixed batch: 500 messages, 5% bad checksums
static hftu::RegisterBenchmark reg_solution(
    "BM_Solution", 500,
    [](int iterations) -> uint64_t {
        auto entries = build_entries();
        auto batch = generate_batch(500, 0xF1A1);

        // Compute ops = total KB
        // We report cycles/KB so ops_per_iteration = batch KB (approx)
        // Harness computes: total_cycles / (iterations * ops_per_iteration)

        uint64_t total = 0;
        for (int i = 0; i < iterations; ++i) {
            hftu::FixParser parser;
            parser.build(entries);

            std::vector<hftu::ParsedOrder> out;
            out.reserve(batch.message_count);

            uint64_t start = hftu::cycle_start();
            parser.parse_batch(batch.data, out);
            hftu::clobber();
            uint64_t end = hftu::cycle_end();
            total += (end - start);

            hftu::do_not_optimize(out);
        }
        return total;
    }
);

int main() {
    // Override ops_per_iteration to reflect KB
    // We generate and measure, then print
    auto entries = build_entries();
    auto batch = generate_batch(500, 0xF1A1);
    size_t kb = (batch.data.size() + 1023) / 1024;

    // Patch the registered benchmark's ops to KB
    auto& reg = hftu::benchmark_registry();
    for (auto& def : reg) {
        def.ops_per_iteration = kb;
    }

    hftu::run_benchmarks();
    return 0;
}
