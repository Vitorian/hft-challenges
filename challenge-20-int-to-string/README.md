# Challenge 20: Fast Integer-to-String

Convert a `uint64_t` to decimal ASCII as fast as possible — the hot path of
every FIX encoder and order gateway.

Implement in `solution/`:

```cpp
namespace hftu {
// Write the decimal representation of value into buf.
// Return the number of characters written (1..20).
size_t u64_to_chars(uint64_t value, char* buf);
}
```

Contract:

- Digits only: no sign, no leading zeros (`0` encodes as `"0"`), no null
  terminator required.
- `buf` always has **32 writable bytes**; bytes beyond the returned length
  are scratch. Never write outside the 32.
- Output is validated byte-exactly against a reference, including edge
  cases (0, `UINT64_MAX`, every power of ten).

The benchmark converts a stream of values whose digit count is uniform in
1–20 (then uniform within that decimal range) — every output length is
equally likely. Score is average cycles per conversion.

Build and run locally:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/benchmark
```

The certified run on the server uses a different seed. See the challenge
page on [hftuniversity.com](https://hftuniversity.com/challenges) for
submission instructions and the leaderboard.
