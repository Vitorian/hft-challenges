# FPGA 01: Cut-Through UDP Echo (HLS)

The same challenge as [FPGA 01 (RTL)](../fpga-01-udp-echo/README.md) — read it for the contract,
the echo rules and the tests — written in C++ with **Vitis HLS**. It has its own leaderboard and
badges.

Implement in `solution/`:

```cpp
#include "udp_echo.h"
void udp_echo(hls::stream<beat_t>& in, hls::stream<beat_t>& out);   // beat_t = ap_axiu<64, 1, 0, 0>
```

Keep the top-level pragmas of the starting point: `ap_ctrl_none` (the function runs freely, one
call per clock) and `axis` on both streams. `build.sh` wraps the generated RTL in the same
`fpga01_echo` module the RTL lane uses, so both lanes go through the same tests and timing.

Two things specific to HLS:

- **The input cannot be stalled.** A function that reads a whole frame and then writes it back would
  stop reading while it writes, and the beats that arrive meanwhile are lost. The starting point is
  a free-running pipeline: every call accepts the incoming beat (if any) and sends one beat (if any).
- **Pipeline depth counts.** Every stage HLS inserts adds a cycle of latency; the generated
  interface register slices count too.

## What solution/ may contain

A flat directory (no subdirectories, no links) of C++ sources and headers (`.cpp`, `.cc`, `.h`,
`.hpp`, ...) and notes (`.md`, `.txt`), up to 50 files; the grader compiles `udp_echo.cpp` with the
lane's `hls_config.cfg`. It rejects a submission whose sources use `#include` of anything but a file
in `solution/`, the AMD `ap_*`/`hls_*` headers or the common standard headers (`<cstdint>`,
`<algorithm>`, ...), a `#pragma` other than `HLS` and `once`, or `__has_include`/`_Pragma`.

## Scoring

Worst first-beat-in to first-beat-out latency over the hidden tests, as in the RTL lane. Badges:

| Gold | Silver | Bronze |
|---|---|---|
| ≤ 8 cycles | ≤ 12 cycles | ≤ 32 cycles |

Plus every hidden test passing and timing closed at 156.25 MHz on `xcu50-fsvh2104-2-e`. The grader
synthesizes with **Vitis HLS 2026.1**.

The starting point in `solution/` is correct but slow (store and forward, ~280 cycles worst case).

## Build and test locally

```bash
./build.sh            # Vitis HLS on your machine: solution/ -> build/rtl (generated Verilog + wrapper)
./test.sh             # the FPGA 01 tests on build/rtl, in the simulation container (Podman or Docker)
./test.sh verilator   # faster
./timing.sh           # optional: Vivado timing of build/rtl
```

Vitis HLS is a free AMD download (a large install, and 2026.1 needs a free licence file): see
[common/fpga/SETUP.md](../common/fpga/SETUP.md). The simulation container has no AMD tools
in it, so `build.sh` runs on your machine and `test.sh` simulates the RTL it produced.

See the challenge page on [hftuniversity.com](https://hftuniversity.com/challenges) for submission
instructions.
