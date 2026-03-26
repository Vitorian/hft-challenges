#pragma once
#include <cstdint>

struct Message {
    int64_t  timestamp;
    double   price;
    double   quantity;
    uint32_t symbol_id;
    uint16_t type;       // 0=trade, 1=quote_bid, 2=quote_ask, 3=cancel, 4=special
    uint8_t  flags;      // bit 0: urgent, bit 1: special processing
    uint8_t  _pad;
};

struct Result {
    double   vwap;
    double   spread;
    double   score;
    double   values[8];  // intermediate computation results
    uint32_t symbol_id;
    uint16_t classification;
    uint8_t  processed;
    uint8_t  _pad;
};
