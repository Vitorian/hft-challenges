# FPGA 02: Low-Latency 10GBASE-R Receive Path

Every nanosecond a trading FPGA spends between the fibre and its logic is spent in the PHY and the
MAC. On our bench, an open-source PHY/MAC's transmit + receive round trip through its own transceiver
is ~240 ns, and the transceiver's analog part (serializer, deserializer, clock recovery) is only ~45 ns
of it: the rest is digital logic. Nobody publishes how small the receive side can get. Build the
fastest correct one.

Implement in `solution/` (Verilog or SystemVerilog, any number of files):

```verilog
module fpga02_rx #(
    parameter W = 64                  // 32 or 64: your choice
) (
    input  wire           clk,        // W=32: 322.265625 MHz (3.103 ns), W=64: 161.1328125 MHz (6.206 ns)
    input  wire           rst,        // active high, synchronous
    input  wire [W-1:0]   rx_data,    // raw bits from the deserializer, rx_data[0] = earliest bit
    output wire           block_lock,
    output wire [W-1:0]   m_tdata,    // received frames: byte 0 in bits 7:0, no preamble/SFD, no FCS
    output wire [W/8-1:0] m_tkeep,
    output wire           m_tvalid,
    output wire           m_tlast,
    output wire           m_tuser     // on the last beat: the frame is bad
);
```

`rx_data` is what a transceiver in raw mode delivers: the line bits of IEEE 802.3 clause 49
(10GBASE-R) at 10.3125 Gb/s, W per clock, **at an arbitrary bit alignment**. You own everything from
there to a MAC receive stream:

- **Block alignment and lock (clause 49):** find the 66-bit block boundary from any offset (sync
  header `01` or `10`); 64 valid headers in a row to lock; lose lock on too many invalid headers and
  find it again.
- **Descrambling:** the self-synchronizing x^58 + x^39 + 1 scrambler over each block's 64 payload
  bits (the 2-bit sync header is not scrambled).
- **64b/66b decoding:** data and control blocks, start of frame in lane 0 or lane 4, terminate in
  any lane, idles and ordered sets. A block with an invalid sync header, block type or control code
  is an error block.
- **MAC receive:** skip preamble and SFD, output the frame bytes without the FCS, check the FCS
  (CRC-32), and flag the frame bad if the FCS is wrong or an error (`/E/` or an error block) occurs
  inside it.

Line bit order: sync header bit 0, bit 1, then payload bits 0..63 (bit 0 first); in a control block
payload bits 7:0 are the block type; data lane n is payload bits 8n+7:8n.

## Contract

- **No back-pressure:** output beats of a frame must be contiguous (a cut-through consumer would run
  dry otherwise). `m_tkeep` is all ones except a contiguous low mask on the last beat.
- **Good frames:** every frame the reference receiver sees as good must come out byte-exact, in
  order, none missing, with `m_tuser` = 0.
- **Bad frames** (bad FCS, `/E/` or an error block inside the frame, broken preamble): drop them, or
  output them with `m_tuser` = 1 on the last beat. Never output one as good.
- **Lock:** `block_lock` must come up before the first frame (the test streams idle for 400 blocks
  first, enough for any clause 49 search). After a deliberate loss of lock, frames that arrive while
  your receiver re-locks are optional.

## What solution/ may contain

A flat directory (no subdirectories, no links) of Verilog/SystemVerilog sources (`.v`, `.sv`, `.vh`,
`.svh`), memory-init files (`.dat`, `.mem`, `.hex`) and notes (`.md`, `.txt`), up to 50 files. The
grader checks the sources before compiling them and rejects a submission that uses:

- system tasks and functions beyond what a design and its debugging need (`$clog2`, `$bits`,
  `$signed`, `$display`, `$finish`, ... are fine; `$fopen`, `$system`, `$dumpfile` and Verilator's
  `$c` are not), or `$readmemh`/`$readmemb` with anything but a quoted name of a file in `solution/`;
- `` `include `` of anything but a file in `solution/`, macro pasting (` `` `), simulator-specific
  directives (`` `systemc_* ``, `` `verilog ``) or DPI imports.

The grader builds your design at the **default value of `W`** in your `fpga02_rx` header, so write
it as a plain number: `parameter W = 32` or `parameter W = 64`.

## Scoring

Latency = time from the first bit of frame byte 0 entering on `rx_data` to `m_tdata` first carrying
byte 0 with `m_tvalid`, in your clock's cycles times its period (3.103 or 6.206 ns). The score is the
**worst latency in nanoseconds over every good frame** of the hidden tests: one leaderboard for both
widths. Your design must also pass every hidden test and close timing at its clock on the Alveo U50's
part (`xcu50-fsvh2104-2-e`) with all inputs and outputs registered; resources break ties.

The starting point in `solution/` is correct, closes timing at both widths, and is slow on purpose:
it stores every frame and checks the FCS before sending its first byte (~1.25 µs worst case at either
width).

## Test locally

You need Podman or Docker; no board, no AMD tools:

```bash
./test.sh 64              # W=64, Icarus, public seed (~1 min)
./test.sh 32 verilator    # W=32, faster simulator
./test.sh 64 icarus 7     # another random stream
```

The testbench generates real 10GBASE-R line bits with a Python reference transmitter (the same
block formats and scrambler as a production PHY), feeds them to your module W bits per clock, and
checks your output against an independent reference receiver. The groups: every frame size from 64
to 1518 bytes, minimum inter-packet gaps with ordered sets, bad frames (bad FCS and `/E/` inside
frames), random bit errors on the line, loss and re-acquisition of block lock, and link-up from many
bit offsets. Results go to `results/results_W<W>.json`. The grader runs the same tests with its own
seeds and more traffic.

Timing (optional locally, needs Vivado 2026.1, free for Alveo targets; setup in
[common/fpga/SETUP.md](../common/fpga/SETUP.md)):

```bash
./timing.sh 64            # writes timing/summary_W64.json (WNS, WHS, LUT/FF/BRAM)
```

See the challenge page on [hftuniversity.com](https://hftuniversity.com/challenges) for submission
instructions. Simulation results come back within minutes; timing is confirmed in a nightly run, and
the best receivers are run on real hardware.
