# Challenge 19: Memory Triathlon

Write a **custom allocator** for the three size classes of a limit-order book,
and win three timed legs at once:

```
score = mean(allocate) + mean(free) + mean(access)    // CPU cycles, lower is better
```

## The trace

We replay a recorded **domain-object lifecycle** of an order book driven by
realistic market flow. Three fixed, public size classes:

| class | size | alignment | what it is |
|---|---|---|---|
| `Order` | 64 B | 64 | one resting order (born on add, freed on cancel/fill) |
| `Level` | 128 B | 64 | one price level (born on first order at a price, freed when it empties) |
| `Shard` | 256 B | 64 | one symbol's book (born on first activity) |

Each replayed event is one of: **allocate**(class, hotness), **free**(ptr), or
**access**(ptr) — a timed read of a live object.

## The interface

You implement `tri::Allocator` in `solution/solution.h`:

```cpp
class Allocator {
public:
    explicit Allocator(const ShapeTable& shapes);     // steady-state sizing hint
    void* allocate(Class c, uint8_t hotness);         // >= kSize[c] bytes, kAlign[c]-aligned
    void  deallocate(Class c, void* p);
};
```

- `hotness` (0..10) is the object's **per-symbol** activity hint, constant for
  its life. Co-locate hot objects to win the access leg.
- `shapes` gives the **steady-state** peak live count per class (Order≈825k / Level≈4k / Shard≈128). The certified
  trace adds a secret **surge** that exceeds it — grow your pools gracefully.

## Rules

- Return at least `kSize[c]` bytes, aligned to `kAlign[c]` (64).
- Never hand the same bytes to two live objects; never undersize a block.
- Do not touch a block's contents between `allocate()` and `deallocate()` — the
  harness owns the first and last 8 bytes (integrity tokens). Aliasing,
  undersizing, or dropping a block **rejects** the run.

## Where the points are

1. **allocate / free** — pool the three classes; an O(1) free-list (pop on
   allocate, push on deallocate) crushes `malloc`. Pre-size from the ShapeTable.
2. **access** — keep each hot symbol's working set cache-resident via placement.
3. **surge** — grow lazily under the flood; a handful of one-off grows are lost in the mean.

## Build & run

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release   # also fetches sample.trace (~4 MB)
cmake --build build
./build/benchmark                            # runs on sample.trace
```

The configure step downloads `sample.trace` from this repo's releases. If you're
offline, run `./fetch_sample.sh` manually, or point the harness at any trace via
`TRIATHLON_TRACE=/path/to.trace ./build/benchmark`.

## Files

- `solution/solution.h` — your allocator (**edit this**)
- `solution/solution.cpp` — translation unit (edit if you split out definitions)
- `triathlon.h` — the frozen interface (do NOT modify)
- `replay.hpp`, `benchmark.cpp` — the harness (do NOT modify)
- `fetch_sample.sh` — downloads the local-dev sample trace (CMake runs it for you)
- `sample.trace` — short, surge-free trace for local iteration (fetched, not committed)
- `CMakeLists.txt` — build config (do NOT modify)

> The local `sample.trace` is short and surge-free. The certified run uses a
> different held-out trace (with the surprise surge) and a private integrity
> salt, so your local score will differ from your submitted score.
