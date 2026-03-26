#include "processor.h"
#include "math_ops.h"
#include "build_defs.h"

class MarketProcessor : public BaseProcessor {
public:
    HOT_FUNC void process(const Message& msg, Result& result) override {
        result.vwap = compute_vwap(msg.price, msg.quantity, result.vwap);
        result.spread = compute_spread(msg.price, result.spread);
        for (int i = 0; i < 8; i++) {
            result.values[i] = transform_value(msg.price, msg.quantity, i);
        }
        result.score = compute_score(result.values, 8);
        result.symbol_id = msg.symbol_id;
        result.processed = 1;
    }
};

BaseProcessor* create_processor() {
    static MarketProcessor instance;
    return &instance;
}
