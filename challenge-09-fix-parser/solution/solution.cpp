// Challenge 09: FIX Parser — Skeleton Implementation
// This is a correct but slow reference. You can do MUCH better!

#include "solution.h"

namespace hftu {

FixParser::FixParser() {}

void FixParser::build(std::span<const TickerEntry> entries) {
    symbol_map_.reserve(entries.size());
    for (const auto& e : entries) {
        symbol_map_.emplace(std::string(e.symbol), e.value);
    }
}

// Parse a fixed-point decimal string like "187.50" into int64_t * 10^8
static int64_t parse_price(std::string_view s) {
    int64_t whole = 0;
    int64_t frac = 0;
    int frac_digits = 0;
    bool in_frac = false;

    for (char c : s) {
        if (c == '.') {
            in_frac = true;
        } else if (c >= '0' && c <= '9') {
            if (in_frac) {
                frac = frac * 10 + (c - '0');
                ++frac_digits;
            } else {
                whole = whole * 10 + (c - '0');
            }
        }
    }
    int64_t result = whole * 100'000'000LL;
    for (int i = frac_digits; i < 8; ++i) frac *= 10;
    for (int i = 8; i < frac_digits; ++i) frac /= 10;
    return result + frac;
}

static int64_t parse_int(std::string_view s) {
    int64_t val = 0;
    for (char c : s) {
        if (c >= '0' && c <= '9') val = val * 10 + (c - '0');
    }
    return val;
}

static int compute_checksum(const char* data, size_t len) {
    int sum = 0;
    for (size_t i = 0; i < len; ++i) {
        sum += static_cast<unsigned char>(data[i]);
    }
    return sum & 0xFF;
}

void FixParser::parse_batch(std::string_view data, std::vector<ParsedOrder>& out) {
    size_t pos = 0;

    while (pos < data.size()) {
        // Find tag 10= (always last tag)
        auto tag10 = data.find("10=", pos);
        if (tag10 == std::string_view::npos) break;

        auto msg_end = data.find('\x01', tag10);
        if (msg_end == std::string_view::npos) break;
        ++msg_end;

        std::string_view msg = data.substr(pos, msg_end - pos);
        ParsedOrder order{};

        // Checksum: sum of all bytes before "10=" mod 256
        std::string_view cs_str = msg.substr(tag10 - pos + 3, msg_end - tag10 - 4);
        int expected_cs = 0;
        for (char c : cs_str) {
            if (c >= '0' && c <= '9') expected_cs = expected_cs * 10 + (c - '0');
        }
        int actual_cs = compute_checksum(msg.data(), tag10 - pos);
        order.valid = (actual_cs == expected_cs);

        // Scan tags
        size_t tpos = 0;
        while (tpos < msg.size()) {
            auto eq = msg.find('=', tpos);
            if (eq == std::string_view::npos) break;
            auto soh = msg.find('\x01', eq);
            if (soh == std::string_view::npos) soh = msg.size();

            // Parse tag number
            int tag = 0;
            for (size_t i = tpos; i < eq; ++i) {
                char c = msg[i];
                if (c >= '0' && c <= '9') tag = tag * 10 + (c - '0');
            }

            std::string_view value = msg.substr(eq + 1, soh - eq - 1);

            switch (tag) {
                case 35: if (!value.empty()) order.msg_type = value[0]; break;
                case 54: if (!value.empty()) order.side = static_cast<int8_t>(value[0] - '0'); break;
                case 55: {
                    auto it = symbol_map_.find(std::string(value));
                    if (it != symbol_map_.end()) order.symbol_id = it->second;
                    break;
                }
                case 44: order.price = parse_price(value); break;
                case 38: order.quantity = parse_int(value); break;
                default: break;
            }

            tpos = soh + 1;
        }

        out.push_back(order);
        pos = msg_end;
    }
}

} // namespace hftu
