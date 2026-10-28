# FPGA 01: Cut-Through UDP Echo (RTL)

Echo UDP packets back to the sender with as few clock cycles as possible between the first byte in
and the first byte out — the ping of every exchange gateway and the first thing a low-latency NIC
has to get right.

Implement in `solution/` (Verilog or SystemVerilog, any number of files):

```verilog
module fpga01_echo (
    input  wire        clk,        // 156.25 MHz (6.4 ns), the 10G MAC datapath clock
    input  wire        rst,        // active high, synchronous
    input  wire [63:0] s_tdata,    // received frame, one 64-bit beat per clock
    input  wire [7:0]  s_tkeep,
    input  wire        s_tvalid,
    input  wire        s_tlast,
    input  wire        s_tuser,
    output wire [63:0] m_tdata,    // frame to transmit
    output wire [7:0]  m_tkeep,
    output wire        m_tvalid,
    output wire        m_tlast,
    output wire        m_tuser
);
```

The module sits between a 10G Ethernet MAC's receive and transmit streams. A frame is what the MAC
hands over: Ethernet header onwards, no preamble, no FCS (the transmit MAC appends it).

## Contract

- **Bytes:** byte 0 of the frame is `tdata[7:0]` of the first beat, byte 7 is `tdata[63:56]`.
  `tkeep` is all ones except on the last beat, where it is a contiguous low mask.
- **Input cannot be stalled:** there is no `tready`. A beat arrives whenever `s_tvalid` is high, and
  frames can arrive back to back with no idle cycle between them.
- **Output must not pause inside a frame:** once a frame's first beat is out, its beats must follow
  on consecutive cycles until `m_tlast` (the transmit MAC aborts a frame that runs dry).
- **Echo exactly these frames:** EtherType `0x0800`, IPv4 with no options (byte 14 = `0x45`),
  protocol 17 (UDP), not fragmented (`flags/offset & 0x3FFF == 0`), destination
  **192.168.1.128:1234**.
- **The echo:** swap the Ethernet destination and source MACs (bytes 0-5 / 6-11), the IPv4 source
  and destination addresses (26-29 / 30-33) and the UDP source and destination ports (34-35 /
  36-37). Change nothing else: swapping source and destination leaves the IP and UDP checksums
  valid, so no checksum arithmetic is needed.
- **Drop everything else:** other ports or addresses, TCP, ARP, VLAN tags, IPv6, IP options,
  fragments, runts.
- **Bad frames:** a frame with `s_tuser` set on its last beat failed its FCS check. Drop it, or echo
  it with `m_tuser` set on its last beat (so the transmit MAC aborts it). Never echo it as good.

## What solution/ may contain

A flat directory (no subdirectories, no links) of Verilog/SystemVerilog sources (`.v`, `.sv`, `.vh`,
`.svh`), memory-init files (`.dat`, `.mem`, `.hex`) and notes (`.md`, `.txt`), up to 50 files. The
grader checks the sources before compiling them and rejects a submission that uses:

- system tasks and functions beyond what a design and its debugging need (`$clog2`, `$bits`,
  `$signed`, `$display`, `$finish`, ... are fine; `$fopen`, `$system`, `$dumpfile` and Verilator's
  `$c` are not), or `$readmemh`/`$readmemb` with anything but a quoted name of a file in `solution/`;
- `` `include `` of anything but a file in `solution/`, macro pasting (` `` `), simulator-specific
  directives (`` `systemc_* ``, `` `verilog ``) or DPI imports.

## Scoring

Latency = cycles from the first beat of a frame entering (`s_tvalid` with byte 0) to the first beat
of its echo leaving (`m_tvalid` with byte 0): 0 for a combinational path, +1 per register stage. The
score is the **worst latency over every echoed frame** of the hidden tests. Badges:

| Gold | Silver | Bronze |
|---|---|---|
| ≤ 6 cycles | ≤ 8 cycles | ≤ 32 cycles |

Every badge also requires passing every hidden test and closing timing at 156.25 MHz on the Alveo
U50's part (`xcu50-fsvh2104-2-e`), with all inputs and outputs registered (so a combinational path
through your module is timed, not free).

The starting point in `solution/` is correct but slow: it stores every frame completely before
sending it back (~280 cycles worst case). The hint that matters is in the contract above: you do not
have to wait for the end of the frame.

## Test locally

You need Podman or Docker; no board, no AMD tools:

```bash
./test.sh              # Icarus Verilog, public seed (~30 s)
./test.sh verilator    # faster
./test.sh icarus 7     # another random stream
```

`test.sh` builds a small simulation image (cocotb, Icarus, Verilator) on first use and writes
`results/results.json`. The tests cover every UDP payload size (18..1472 bytes), back-to-back
frames, minimum-size frames faster than line rate, frames that must be dropped, bad frames, and a
random mix. The grader runs the same tests with its own seeds and more traffic.

Timing (optional locally, needs Vivado 2026.1, free for Alveo targets; setup in
[common/fpga/SETUP.md](../common/fpga/SETUP.md)):

```bash
./timing.sh            # writes timing/summary.json (WNS, WHS, LUT/FF/BRAM)
```

See the challenge page on [hftuniversity.com](https://hftuniversity.com/challenges) for submission
instructions. Simulation results come back within minutes; timing is confirmed in a nightly run.
Top entries are also run on real hardware (an Alveo U50 measured by an X3522PV test card) and the
measured wire-to-wire numbers are published next to the scores.
