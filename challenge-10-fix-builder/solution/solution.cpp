// Challenge 10: FIX Encoder — Skeleton Implementation
// This is a correct but slow reference. You can do MUCH better!
// The key insight: pre-serialize everything in build(), then only patch
// the variable fields and recompute BodyLength + CheckSum in encode().

#include "solution.h"
#include <cstdio>
#include <cstring>
#include <ctime>

namespace hftu {

FixBuilder::FixBuilder() {
    buffer_.reserve(1024);
}

void FixBuilder::build(std::span<const std::pair<int, std::string>> fields) {
    template_fields_.assign(fields.begin(), fields.end());
}

// Format nanoseconds from epoch to FIX timestamp: YYYYMMDD-HH:MM:SS.nnnnnnnnn
static std::string format_timestamp(int64_t ns_epoch) {
    int64_t secs = ns_epoch / 1'000'000'000LL;
    int64_t nanos = ns_epoch % 1'000'000'000LL;

    time_t t = static_cast<time_t>(secs);
    struct tm tm;
    gmtime_r(&t, &tm);

    char buf[32];
    int len = std::snprintf(buf, sizeof(buf), "%04d%02d%02d-%02d:%02d:%02d.%09ld",
                            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                            tm.tm_hour, tm.tm_min, tm.tm_sec, (long)nanos);
    return std::string(buf, len);
}

// Format price from fixed-point (10^8) to decimal string
static std::string format_price(int64_t price) {
    int64_t whole = price / 100'000'000LL;
    int64_t frac = price % 100'000'000LL;
    char buf[32];
    // Remove trailing zeros from fractional part
    if (frac == 0) {
        int len = std::snprintf(buf, sizeof(buf), "%ld.0", (long)whole);
        return std::string(buf, len);
    }
    int len = std::snprintf(buf, sizeof(buf), "%ld.%08ld", (long)whole, (long)frac);
    // Trim trailing zeros
    while (len > 0 && buf[len - 1] == '0') --len;
    return std::string(buf, len);
}

std::string_view FixBuilder::encode(const OrderFields& order) {
    static constexpr char SOH = '\x01';

    // Build body (everything between BeginString+BodyLength and CheckSum)
    std::string body;
    body.reserve(512);

    std::string begin_string;

    for (const auto& [tag, value] : template_fields_) {
        if (tag == 8) {
            begin_string = value;  // save for header
            continue;
        }
        body += std::to_string(tag);
        body += '=';
        body += value;
        body += SOH;
    }

    // Add variable fields
    body += "34="; body += std::to_string(order.seq_num); body += SOH;
    body += "52="; body += format_timestamp(order.timestamp); body += SOH;
    body += "11="; body += std::to_string(order.cl_ord_id); body += SOH;
    body += "54="; body += std::to_string(static_cast<int>(order.side)); body += SOH;
    body += "44="; body += format_price(order.price); body += SOH;
    body += "38="; body += std::to_string(order.quantity); body += SOH;

    // Build header
    std::string header;
    header += "8=";
    header += begin_string.empty() ? "FIX.4.4" : begin_string;
    header += SOH;
    header += "9=";
    header += std::to_string(body.size());
    header += SOH;

    // Compute checksum
    std::string pre_checksum = header + body;
    int sum = 0;
    for (unsigned char c : pre_checksum) sum += c;
    char cs_buf[8];
    std::snprintf(cs_buf, sizeof(cs_buf), "%03d", sum & 0xFF);

    buffer_.clear();
    buffer_ += pre_checksum;
    buffer_ += "10=";
    buffer_ += cs_buf;
    buffer_ += SOH;

    return buffer_;
}

} // namespace hftu
