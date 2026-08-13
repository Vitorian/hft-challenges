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

## The workload: an execution-report field mix

The benchmark models the integer fields an order gateway actually converts —
the numeric-field census of a FIX ExecutionReport (~4 quantities, 3 prices,
3 IDs, a sequence number, timestamps). Each conversion draws one field,
i.i.d.:

| Weight | Field | Values | Digits |
|---|---|---|---|
| 30% | Quantity (OrderQty, LastQty, LeavesQty, CumQty) | log-uniform [0, 10^6) | 1–6 |
| 25% | Price, 4 implied decimals (Price, LastPx, AvgPx) | log-uniform [10^4, 10^8) — a $1–$10,000 price scaled ×10^4 | 5–8 |
| 25% | Order / exec ID | log-uniform [10^9, 10^19) | 10–19 |
| 10% | Sequence number | uniform [1, 10^10] — a random message in a 10-billion-message day | mostly 10 |
| 10% | Timestamp | uniform nanoseconds-since-midnight over the 09:30–16:00 session | always 14 |

"Log-uniform" means the digit count is drawn uniformly over the field's
range, then the value uniformly within that decade — every length a field
can produce is equally likely. The resulting per-digit distribution
(exactly reproduced by `make_values` in `benchmark.cpp`):

```
digits:  1-4    5     6     7     8    9    10   11-13   14   15-19   20
share:  5% ea 11.3% 11.3% 6.3%  6.3% 0.9% 11.5% 2.5%ea 12.5% 2.5%ea  0%
```

Grounding: quantity weights reflect that odd lots (<100 shares) are now the
majority of US equity trades (SEC MIDAS / exchange data, ~60%+ since the
2020s); price encoding follows the Nasdaq TotalView-ITCH 5.0 `Price(4)`
fixed-point format (4 implied decimals); order reference numbers are 8-byte
unsigned integers in ITCH, and epoch-nanosecond-derived IDs reach 19
digits; ITCH timestamps are nanoseconds since midnight. Note that 20-digit
values never appear in the scored stream — no real field produces them —
but correctness at every length **including 20 digits** is still validated.
Specializing your code for this mix is fair game and entirely the point:
it is stated precisely so that everyone optimizes against the same traffic
model, the way a production encoder is tuned for real traffic.

Conversions run in batches of 16 independent values, each into its own
32-byte slot — like an encoder emitting the numeric fields of one message
back-to-back — so consecutive calls can overlap in the pipeline. The
lengths are i.i.d. random per call: there is no sequence for the branch
predictor to memorize. Score is average cycles per conversion.

Build and run locally:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/benchmark
```

The certified run on the server uses a different seed. See the challenge
page on [hftuniversity.com](https://hftuniversity.com/challenges) for
submission instructions and the leaderboard.
