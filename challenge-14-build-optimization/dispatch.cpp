#include "dispatch.h"
#include "build_defs.h"

HOT_FUNC int classify_message(const Message& msg) {
    if (UNLIKELY(msg.flags & 0x02))
        return 4;
    if (UNLIKELY(msg.type == 4))
        return 3;
    if (msg.type == 3)
        return 2;
    if (LIKELY(msg.type <= 1))
        return 0;
    return 1;
}

void route_message(const Message& msg, Result& result, int classification) {
    switch (classification) {
        case 0: result.classification = 0; break;
        case 1: result.classification = 1; break;
        case 2: result.classification = 2; break;
        case 3: result.classification = 3; break;
        case 4: result.classification = 4; break;
        default: result.classification = 0xFF; break;
    }
}
