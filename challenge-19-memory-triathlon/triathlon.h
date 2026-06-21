#pragma once
// Memory Triathlon — public allocator interface (the frozen contract).
//
// The solver implements ONE thing: a custom allocator that serves three fixed
// size classes, given a steady-state ShapeTable at construction. The grading
// harness replays a recorded order-book allocation trace through it and times
// three legs:
//
//     score = mean(allocate) + mean(free) + mean(access)   [cycles, lower better]
//
// The three classes are the domain objects of a pointer-based limit-order book:
//   Order (64 B)  — one resting order
//   Level (128 B) — one price level (a queue of orders)
//   Shard (256 B) — one symbol's book (its bid/ask level maps)
//
// HOTNESS is a stable PER-SYMBOL property (0..10): all of a busy symbol's
// objects are read far more, throughout their lives. It is handed to the
// allocator at allocate() time and never changes for that object. The winning
// strategy co-locates a hot symbol's Orders+Levels+Shard so the hot working set
// stays L1/L2-resident. Hotness is only a HINT — ignoring it is legal (and is
// what the baseline does), it just loses the access leg.
//
// This header is frozen and public. The solver provides `tri::Allocator` in
// solution.h (see the required member signatures at the bottom).

#include <cstddef>
#include <cstdint>

namespace tri {

enum class Class : uint8_t { Order = 0, Level = 1, Shard = 2 };
inline constexpr int      kNumClasses = 3;
inline constexpr uint32_t kSize[3]  = { 64, 128, 256 };   // class -> served bytes
inline constexpr uint32_t kAlign[3] = { 64,  64,  64 };   // class -> alignment

inline constexpr const char* class_name(Class c) {
    switch (c) {
        case Class::Order: return "Order";
        case Class::Level: return "Level";
        case Class::Shard: return "Shard";
    }
    return "?";
}

// Steady-state sizing hints handed to the allocator at construction. Each known
// regime gives the expected peak LIVE count per class. The out-of-distribution
// SURGE is deliberately NOT represented here — the allocator must size pools
// for the regimes it is told about and still grow gracefully when a surge
// blows past them (one-shot grows are lost in the mean; a fixed-capacity
// allocator that faults or degrades under surge loses).
struct ShapeTable {
    static constexpr int kMaxRegimes = 4;
    int      count = 0;                       // number of valid regimes
    uint32_t live[kMaxRegimes][kNumClasses] = {};  // [regime][class] -> peak live

    // Convenience: the largest told-about live count for a class (across
    // regimes) — a reasonable initial reservation target.
    uint32_t max_live(Class c) const {
        uint32_t m = 0;
        for (int r = 0; r < count; ++r)
            if (live[r][int(c)] > m) m = live[r][int(c)];
        return m;
    }
};

// ---- The contract the solver implements in solution.h ----
//
//   class Allocator {
//   public:
//       explicit Allocator(const ShapeTable& shapes);
//
//       // Return a block of at least kSize[c] bytes, aligned to kAlign[c].
//       // `hotness` (0..10) is the object's symbol hotness hint. Must never
//       // return a block that overlaps a currently-live block (the integrity
//       // check will catch it). Returning nullptr is a failure.
//       void* allocate(Class c, uint8_t hotness);
//
//       // Return a block previously handed out by allocate(c, ...) and not yet
//       // freed. The harness has already verified its integrity token.
//       void  deallocate(Class c, void* p);
//   };
//
// The harness owns the first and last 8 bytes of every block (integrity
// tokens); the allocator must not touch a block's contents between allocate
// and deallocate, nor hand the same bytes to two live objects.

} // namespace tri
