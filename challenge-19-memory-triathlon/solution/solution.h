#pragma once
// Memory Triathlon — YOUR allocator. Edit this file.
//
// You implement ONE thing: a custom allocator that serves three fixed size
// classes (Order 64B / Level 128B / Shard 256B). The harness replays a recorded
// order-book allocation trace through it and times three legs:
//
//     score = mean(allocate) + mean(free) + mean(access)   [cycles, lower wins]
//
// The baseline below just forwards to aligned malloc/free and ignores the
// hotness hint. It is correct but slow — beat it.
//
// Where the points are:
//   * allocate / free — pool the three size classes; an O(1) free-list (pop on
//     allocate, push on deallocate) crushes malloc. Pre-size from the ShapeTable.
//   * access — `hotness` (0..10) is the object's symbol-activity hint, constant
//     for the object's life. Co-locate hot objects so a busy symbol's working
//     set stays cache-resident; that wins the access tail.
//   * survive the SURGE — the certified trace floods far past the ShapeTable's
//     sizes at a secret moment. Grow your pools lazily (a few large slabs); the
//     one-off growth is lost in the mean, but faulting or degrading is not.
//
// Rules:
//   * Return at least kSize[c] bytes, aligned to kAlign[c] (64).
//   * Never hand the same bytes to two live objects, and never undersize — the
//     harness writes integrity tokens into the first and last 8 bytes of every
//     block and verifies them. Do not touch a block's contents between
//     allocate() and deallocate().

#include "../triathlon.h"
#include <cstdlib>

namespace tri {

class Allocator {
public:
    explicit Allocator(const ShapeTable& shapes) {
        (void)shapes;   // TODO: pre-size your pools from shapes.max_live(Class)
    }

    void* allocate(Class c, uint8_t hotness) {
        (void)hotness;  // TODO: use the hotness hint to place hot objects together
        return std::aligned_alloc(kAlign[int(c)], kSize[int(c)]);
    }

    void deallocate(Class /*c*/, void* p) {
        std::free(p);
    }
};

} // namespace tri
