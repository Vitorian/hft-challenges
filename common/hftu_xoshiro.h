#pragma once
// hftu_xoshiro.h — fast, bit-exact C++ port of the Rust Rng in hftu-bench.
//
// Why: the previous C++ benchmark harness used std::mt19937_64 +
// std::uniform_int_distribution inside timed loops. That adds 10-25
// cycles per call, which is baked into cycles/op scores. The Rust side
// uses xoshiro256++ at ~5 cycles per call. This port brings C++ and
// Rust benchmarks onto the same measurement framework.
//
// Algorithm: xoshiro256++ (Blackman & Vigna). 32-byte state, full 2^256-1
// period, passes PractRand and BigCrush. See https://prng.di.unimi.it/
//
// This header is self-testing: compile-time assertions check that
// Rng(42).next_u64() produces the exact same sequence as the Rust
// reference implementation. Any drift is caught at build time.

#include <array>
#include <cstdint>

namespace hftu {

class Rng {
public:
    // SplitMix64-seeded xoshiro256++ state. Matches hftu-bench Rust Rng::new.
    explicit constexpr Rng(uint64_t seed) : s_{} {
        uint64_t z = seed;
        for (uint64_t& slot : s_) {
            z += 0x9e3779b97f4a7c15ULL;
            uint64_t x = z;
            x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
            x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
            slot = x ^ (x >> 31);
        }
    }

    // xoshiro256++ step. ~5 cycles on Zen 2 / GCC 13+.
    [[gnu::always_inline]] constexpr uint64_t next_u64() {
        const uint64_t result = rotl(s_[0] + s_[3], 23) + s_[0];
        const uint64_t t = s_[1] << 17;
        s_[2] ^= s_[0];
        s_[3] ^= s_[1];
        s_[1] ^= s_[2];
        s_[0] ^= s_[3];
        s_[2] ^= t;
        s_[3] = rotl(s_[3], 45);
        return result;
    }

    // Unbiased-enough uniform on [0, bound). Lemire's fast reduction.
    // ~8-10 cycles on Zen 2 (one 128-bit multiply + shift).
    [[gnu::always_inline]] constexpr uint64_t next_range(uint64_t bound) {
        const uint64_t x = next_u64();
        return static_cast<uint64_t>((static_cast<__uint128_t>(x) * bound) >> 64);
    }

    // Inclusive int64 in [lo, hi].
    [[gnu::always_inline]] constexpr int64_t next_i64_range(int64_t lo, int64_t hi) {
        const uint64_t range = static_cast<uint64_t>(hi - lo) + 1;
        return lo + static_cast<int64_t>(next_range(range));
    }

private:
    std::array<uint64_t, 4> s_;

    [[gnu::always_inline]] static constexpr uint64_t rotl(uint64_t x, int k) {
        return (x << k) | (x >> (64 - k));
    }
};

// ---- Compile-time self-test against Rust golden values ----
// Generated from hftu-bench Rust Rng::new(42).next_u64() × 10.
// Also verified: Rng::new(42).next_range(1000) × 10 matches.
// Any drift from the Rust reference breaks the build — on purpose.
namespace detail {
constexpr uint64_t rust_golden_next[10] = {
    0xD0764D4F4476689FULL, 0x519E4174576F3791ULL, 0xFBE07CFB0C24ED8CULL,
    0xB37D9F600CD835B8ULL, 0xCB231C3874846A73ULL, 0x968D9F004E50DE7DULL,
    0x201718FF221A3556ULL, 0x9AE94E070ED8CB46ULL, 0x352CF3DAF095CCC7ULL,
    0xEEEFD63219B4A0D4ULL,
};
constexpr bool self_test_next_u64() {
    Rng r{42};
    for (uint64_t expected : rust_golden_next) {
        if (r.next_u64() != expected) return false;
    }
    return true;
}
static_assert(self_test_next_u64(),
    "hftu::Rng::next_u64 diverges from Rust reference. "
    "Check algorithm / constants in hftu_xoshiro.h.");

constexpr uint64_t rust_golden_range1000[10] = {
    814, 318, 983, 701, 793, 588, 125, 605, 207, 933,
};
constexpr bool self_test_next_range() {
    Rng r{42};
    for (uint64_t expected : rust_golden_range1000) {
        if (r.next_range(1000) != expected) return false;
    }
    return true;
}
static_assert(self_test_next_range(),
    "hftu::Rng::next_range diverges from Rust reference.");
}  // namespace detail

// Default RNG with seed 42 — matches hftu_bench::rng() in Rust.
[[gnu::always_inline]] inline Rng default_rng() { return Rng{42}; }

}  // namespace hftu
