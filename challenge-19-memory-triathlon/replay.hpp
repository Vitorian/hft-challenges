#pragma once
// Memory Triathlon — replay & grading core.
//
// Loads a recorded allocation trace (TRITRC01), replays it through a solver's
// tri::Allocator, times the three legs, runs the address-derived integrity
// check, and reports mean(alloc) + mean(free) + mean(access).
//
// Scoring is the per-leg MEAN (not P99): the per-op rdtsc floor (~tens of cycles,
// quantized to coarse steps) is a constant that cancels in cross-solver ranking,
// and averaging millions of ops gives sub-cycle resolution — so solvers rank by
// real allocator quality instead of clustering on a quantized P99 tail. The mean
// is still surge-forgiving: a handful of one-off slab-grow ops out of millions
// carry negligible weight, while a sustained-slow allocator's mean rises.
//
// Templated on the allocator so there is no vtable on the timed path.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "triathlon.h"
#include "common/benchmark_harness.h"   // hftu::cycle_light

namespace tri {

// ---- Trace format (must match recorder/trace.hpp) ----
enum class Op : uint8_t { Alloc = 0, Free = 1, Access = 2 };
#pragma pack(push, 1)
struct Event { uint8_t op, cls, hotness, _pad; uint32_t handle; };
#pragma pack(pop)
static_assert(sizeof(Event) == 8, "Event must be 8 bytes");

struct Trace {
    std::vector<Event> events;
    uint32_t kSize[3] = {0, 0, 0};
    uint32_t handle_space = 0;        // max handle + 1
};

inline bool load_trace(const char* path, Trace& out) {
    std::FILE* f = std::fopen(path, "rb");
    if (!f) { std::fprintf(stderr, "cannot open %s\n", path); return false; }
    char magic[8];
    uint64_t n = 0;
    if (std::fread(magic, 1, 8, f) != 8 || std::memcmp(magic, "TRITRC01", 8) != 0) {
        std::fprintf(stderr, "%s: bad magic\n", path); std::fclose(f); return false;
    }
    if (std::fread(&n, sizeof(n), 1, f) != 1 ||
        std::fread(out.kSize, sizeof(uint32_t), 3, f) != 3) {
        std::fprintf(stderr, "%s: bad header\n", path); std::fclose(f); return false;
    }
    out.events.resize(n);
    if (n && std::fread(out.events.data(), sizeof(Event), n, f) != n) {
        std::fprintf(stderr, "%s: short read\n", path); std::fclose(f); return false;
    }
    std::fclose(f);
    uint32_t hs = 0;
    for (const Event& e : out.events) if (e.handle != UINT32_MAX && e.handle + 1 > hs) hs = e.handle + 1;
    out.handle_space = hs;
    return true;
}

// ---- Fenced TSC ----
// A single light fence (hftu::cycle_light) keeps the per-op overhead floor low
// enough that the access leg's cache-residency signal (~4-40 cyc) is not
// swamped — cycle_start/cycle_end's full CPUID serialization would bury it. The
// residual constant floor cancels in cross-solver ranking. cycle_light is
// portable (x86 lfence+rdtsc, ARM64 isb+cntvct_el0, else clock_gettime) so this
// builds for local iteration on a Mac; the certified run is always x86.
static inline uint64_t tsc() { return hftu::cycle_light(); }

// ---- Address-derived integrity tokens ----
// head/tail 64-bit words derived from the block ADDRESS, the handle, and a
// private per-run salt. Written at alloc, verified at access and free. An
// allocator that overlaps two live blocks corrupts one's tokens; one that
// undersizes a block writes its tail into a neighbour. Never a constant fill,
// so aliasing can't hide. The allocator never sees the salt.
static inline uint64_t mix(uint64_t x) {
    x ^= x >> 33; x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33; x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return x ? x : 0x1234567887654321ULL;
}
static inline uint64_t tok_head(const void* p, uint32_t h, uint64_t salt) {
    return mix(uint64_t(reinterpret_cast<uintptr_t>(p)) ^ (uint64_t(h) * 0x9E3779B97F4A7C15ULL) ^ salt);
}
static inline uint64_t tok_tail(const void* p, uint32_t h, uint64_t salt) {
    return mix(uint64_t(reinterpret_cast<uintptr_t>(p)) ^ (uint64_t(h) * 0xD1B54A32D192ED03ULL) ^ ~salt);
}
static inline void write_tokens(void* p, uint32_t sz, uint32_t h, uint64_t salt) {
    uint64_t a = tok_head(p, h, salt), b = tok_tail(p, h, salt);
    std::memcpy(static_cast<char*>(p),               &a, 8);
    std::memcpy(static_cast<char*>(p) + sz - 8,      &b, 8);
}
static inline bool check_tokens(const void* p, uint32_t sz, uint32_t h, uint64_t salt) {
    uint64_t a, b;
    std::memcpy(&a, static_cast<const char*>(p),          8);
    std::memcpy(&b, static_cast<const char*>(p) + sz - 8, 8);
    return a == tok_head(p, h, salt) && b == tok_tail(p, h, salt);
}

struct Result {
    uint64_t n_alloc = 0, n_free = 0, n_access = 0;
    uint32_t alloc_p50 = 0, alloc_p99 = 0;
    uint32_t free_p50 = 0,  free_p99 = 0;
    uint32_t access_p50 = 0, access_p90 = 0, access_p99 = 0;
    double   alloc_mean = 0, free_mean = 0, access_mean = 0;
    uint64_t score = 0;            // round(mean(alloc) + mean(free) + mean(access))
    uint64_t integrity_fails = 0;
    uint64_t null_allocs = 0;
    uint32_t peak_live = 0;
};

static inline uint32_t pct(std::vector<uint32_t>& v, double p) {
    if (v.empty()) return 0;
    std::sort(v.begin(), v.end());
    size_t i = size_t(double(v.size() - 1) * p);
    return v[i];
}

// Replay `tr` through a fresh Alloc(shapes); return timing/integrity results.
template <class Alloc>
Result replay(const Trace& tr, const ShapeTable& shapes, uint64_t salt) {
    const uint32_t* SZ = kSize;
    std::vector<void*>   live(tr.handle_space, nullptr);
    std::vector<uint8_t> lcls(tr.handle_space, 0);

    std::vector<uint32_t> t_alloc, t_free, t_access;
    t_alloc.reserve(tr.events.size() / 2);
    t_free.reserve(tr.events.size() / 2);
    t_access.reserve(tr.events.size() / 4);

    Result R;
    uint32_t cur_live = 0;
    Alloc alloc(shapes);

    for (const Event& e : tr.events) {
        if (e.op == uint8_t(Op::Alloc)) {
            Class c = Class(e.cls);
            uint64_t t0 = tsc();
            void* p = alloc.allocate(c, e.hotness);
            uint64_t t1 = tsc();
            t_alloc.push_back(uint32_t(t1 - t0));
            R.n_alloc++;
            if (!p) { R.null_allocs++; continue; }
            write_tokens(p, SZ[e.cls], e.handle, salt);
            live[e.handle] = p; lcls[e.handle] = e.cls;
            if (++cur_live > R.peak_live) R.peak_live = cur_live;
        } else if (e.op == uint8_t(Op::Free)) {
            void* p = live[e.handle];
            if (!p) continue;
            Class c = Class(lcls[e.handle]);
            if (!check_tokens(p, SZ[lcls[e.handle]], e.handle, salt)) R.integrity_fails++;
            uint64_t t0 = tsc();
            alloc.deallocate(c, p);
            uint64_t t1 = tsc();
            t_free.push_back(uint32_t(t1 - t0));
            R.n_free++;
            live[e.handle] = nullptr; cur_live--;
        } else { // Access — timed read of the live block (cache-residency signal)
            void* p = live[e.handle];
            if (!p) continue;
            uint32_t sz = SZ[lcls[e.handle]];
            uint64_t a, b;
            uint64_t t0 = tsc();
            std::memcpy(&a, static_cast<const char*>(p), 8);            // first line
            std::memcpy(&b, static_cast<const char*>(p) + sz - 8, 8);  // last line
            uint64_t t1 = tsc();
            uint32_t dt = uint32_t(t1 - t0);
            t_access.push_back(dt);
            R.n_access++;
            if (a != tok_head(p, e.handle, salt) || b != tok_tail(p, e.handle, salt))
                R.integrity_fails++;
        }
    }

    auto mean = [](const std::vector<uint32_t>& v) -> double {
        if (v.empty()) return 0.0;
        double s = 0; for (uint32_t x : v) s += x;
        return s / double(v.size());
    };
    R.alloc_mean = mean(t_alloc);  R.free_mean = mean(t_free);  R.access_mean = mean(t_access);
    R.alloc_p50 = pct(t_alloc, 0.50);  R.alloc_p99 = pct(t_alloc, 0.99);
    R.free_p50  = pct(t_free, 0.50);   R.free_p99  = pct(t_free, 0.99);
    R.access_p50 = pct(t_access, 0.50); R.access_p90 = pct(t_access, 0.90);
    R.access_p99 = pct(t_access, 0.99);
    R.score = uint64_t(std::llround(R.alloc_mean + R.free_mean + R.access_mean));
    return R;
}

// Human-readable report -> stderr (stdout is reserved for the result JSON).
inline void print_result(const char* label, const Result& R) {
    std::fprintf(stderr, "\n==== %s ====\n", label);
    std::fprintf(stderr, "events: alloc=%llu free=%llu access=%llu  peak_live=%u\n",
                (unsigned long long)R.n_alloc, (unsigned long long)R.n_free,
                (unsigned long long)R.n_access, R.peak_live);
    std::fprintf(stderr, "  allocate  mean=%-7.2f P50=%-6u P99=%-6u\n", R.alloc_mean, R.alloc_p50, R.alloc_p99);
    std::fprintf(stderr, "  free      mean=%-7.2f P50=%-6u P99=%-6u\n", R.free_mean, R.free_p50, R.free_p99);
    std::fprintf(stderr, "  access    mean=%-7.2f P50=%-6u P90=%-6u P99=%-6u\n",
                R.access_mean, R.access_p50, R.access_p90, R.access_p99);
    std::fprintf(stderr, "  SCORE = mean(alloc)+mean(free)+mean(access) = %.2f+%.2f+%.2f = %llu cyc\n",
                R.alloc_mean, R.free_mean, R.access_mean, (unsigned long long)R.score);
    std::fprintf(stderr, "  integrity fails: %llu   null allocs: %llu\n",
                (unsigned long long)R.integrity_fails, (unsigned long long)R.null_allocs);
}

// Result JSON -> stdout, in the worker's expected shape. On a correctness
// failure (aliasing / undersize / drop) emit {"error":...} so the run is
// rejected rather than scored.
inline void print_json(const char* bench_name, const Result& R) {
    if (R.integrity_fails || R.null_allocs) {
        std::printf("{\"error\":\"correctness check failed: %llu integrity fails, "
                    "%llu null allocs\"}\n",
                    (unsigned long long)R.integrity_fails,
                    (unsigned long long)R.null_allocs);
        return;
    }
    std::printf("{\"benchmarks\":[{\"name\":\"%s\",\"cycles_per_op\":%llu,"
                "\"alloc_mean\":%.2f,\"free_mean\":%.2f,\"access_mean\":%.2f,"
                "\"alloc_p99\":%u,\"free_p99\":%u,\"access_p99\":%u,\"peak_live\":%u}]}\n",
                bench_name, (unsigned long long)R.score,
                R.alloc_mean, R.free_mean, R.access_mean,
                R.alloc_p99, R.free_p99, R.access_p99, R.peak_live);
}

} // namespace tri
