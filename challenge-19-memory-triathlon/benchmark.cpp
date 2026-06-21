// Memory Triathlon — benchmark / dev driver.  (Do NOT modify.)
//
// Replays an allocation trace through YOUR allocator (solution/solution.h) and
// reports  mean(allocate) + mean(free) + mean(access)  [cycles].  Result JSON goes
// to stdout (what the grader reads); a human-readable report goes to stderr.
//
// Locally this runs on the bundled (fetched) sample.trace. The certified run on
// the server uses a different, held-out trace with a surprise out-of-distribution
// surge and a private integrity salt — so don't overfit to the sample, and never
// alias/undersize/drop a block (the integrity check rejects the run).
//
// All configuration is baked in by CMake (so the public and certified builds
// share one source, differing only in build flags):
//   DEFAULT_TRACE                  path to the trace
//   DEFAULT_SALT                   integrity salt
//   SHAPE_ORDER/LEVEL/SHARD        the told-about steady-state peak live counts
// and may be overridden at runtime via argv or the TRIATHLON_TRACE/SALT env vars.

#include "solution/solution.h"   // your tri::Allocator
#include "replay.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

using namespace tri;

#ifndef DEFAULT_TRACE
#define DEFAULT_TRACE "sample.trace"
#endif
#ifndef DEFAULT_SALT
#define DEFAULT_SALT 0x5A1700D0FACEULL
#endif
// The public, told-about steady-state regime (the surge exceeds it at grade time).
#ifndef SHAPE_ORDER
#define SHAPE_ORDER 825000
#endif
#ifndef SHAPE_LEVEL
#define SHAPE_LEVEL 4000
#endif
#ifndef SHAPE_SHARD
#define SHAPE_SHARD 128
#endif
#ifndef BENCH_NAME
#define BENCH_NAME "BM_Triathlon"
#endif

static const char* arg_or_env(int argc, char** argv, int i, const char* env) {
    if (argc > i) return argv[i];
    return std::getenv(env);
}

int main(int argc, char** argv) {
    const char* tp = arg_or_env(argc, argv, 1, "TRIATHLON_TRACE");
    const char* trace_path = tp ? tp : DEFAULT_TRACE;
    const char* ss = arg_or_env(argc, argv, 2, "TRIATHLON_SALT");
    uint64_t salt = ss ? std::strtoull(ss, nullptr, 0) : (uint64_t)DEFAULT_SALT;

    Trace tr;
    if (!load_trace(trace_path, tr)) {
        std::fprintf(stderr,
            "\nNo trace at '%s'. CMake fetches the sample into the build dir on\n"
            "configure; if you're offline run  ./fetch_sample.sh  (writes\n"
            "build/sample.trace), or point TRIATHLON_TRACE at your own .trace file.\n",
            trace_path);
        std::printf("{\"error\":\"no trace at %s\"}\n", trace_path);
        return 1;
    }

    // The ShapeTable is public knowledge: the steady-state peak live per class.
    // Size your pools for it; the certified trace's surge exceeds it on purpose.
    ShapeTable shapes;
    shapes.count = 1;
    shapes.live[0][0] = SHAPE_ORDER;
    shapes.live[0][1] = SHAPE_LEVEL;
    shapes.live[0][2] = SHAPE_SHARD;

    std::fprintf(stderr, "trace %s: %zu events, peak_live(handle_space)=%u\n",
                 trace_path, tr.events.size(), tr.handle_space);
    std::fprintf(stderr, "ShapeTable (told-about peak live): Order=%u Level=%u Shard=%u\n",
                 shapes.live[0][0], shapes.live[0][1], shapes.live[0][2]);

    Result R = replay<Allocator>(tr, shapes, salt);
    print_result("your allocator", R);   // stderr
    print_json(BENCH_NAME, R);            // stdout (result.json)

    return (R.integrity_fails || R.null_allocs) ? 2 : 0;
}
