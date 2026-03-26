#include "pipeline.h"
#include "processor.h"
#include "dispatch.h"
#include "build_defs.h"

static BaseProcessor* g_processor = nullptr;

void init_pipeline() {
    g_processor = create_processor();
}

HOT_FUNC void process_message(const Message& msg, Result& result) {
    PREFETCH(&result);
    int cls = classify_message(msg);
    route_message(msg, result, cls);

    if (UNLIKELY(cls >= 3)) {
        handle_special(msg, result);
        return;
    }

    g_processor->process(msg, result);
}

COLD_FUNC void handle_special(const Message& msg, Result& result) {
    result.score = -1.0;
    result.processed = 0;
    result.symbol_id = msg.symbol_id;
    try {
        result.vwap = msg.price / (msg.quantity + 1e-9);
    } catch (...) {
        result.vwap = 0;
    }
}

void run_pipeline(const Message* RESTRICT msgs, int count, Result* RESTRICT out) {
    if (!g_processor) init_pipeline();
    for (int i = 0; i < count; i++) {
        PREFETCH(&msgs[i + 4]);
        process_message(msgs[i], out[i]);
    }
}
