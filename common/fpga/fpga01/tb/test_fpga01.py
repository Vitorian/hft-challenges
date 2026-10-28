"""FPGA 01 testbench (cocotb): cut-through UDP echo.

DUT: module fpga01_echo (clk, rst, s_tdata[63:0], s_tkeep[7:0], s_tvalid, s_tlast, s_tuser,
                         m_tdata[63:0], m_tkeep[7:0], m_tvalid, m_tlast, m_tuser)
One 64-bit beat per clock at 156.25 MHz, byte 0 of the frame in bits 7:0. No TREADY: the MAC
cannot be stalled on either side.

Checked on every echoed frame: byte-exact echo (MACs, IPs, ports swapped), valid checksums,
TKEEP, no idle cycle inside an output frame, TUSER. Frames that must not be echoed must not
appear. Latency = (time the first output beat is valid - time the first input beat is valid) / 6.4 ns,
i.e. 0 for a combinational path and 1 per register stage; the score
is the worst latency over every echoed frame of every test.

Environment:
  FPGA01_SEED   random seed (the public tests use 1; the grader uses its own)
  FPGA01_SCALE  multiplies the number of random frames (default 1)
  FPGA01_RESULTS path of the JSON summary (default results.json)
"""
import json
import os
import random

import cocotb
from cocotb.clock import Clock
from cocotb.triggers import ReadOnly, RisingEdge
from cocotb.utils import get_sim_time

import model

SEED = int(os.environ.get("FPGA01_SEED") or "1")
SCALE = int(os.environ.get("FPGA01_SCALE") or "1")
RESULTS = os.environ.get("FPGA01_RESULTS") or "results.json"

PERIOD_PS = 6400


def now_ps():
    return get_sim_time("ps")


SUMMARY = {"seed": SEED, "scale": SCALE, "tests": {}, "latency_max": None, "latency_min": None,
           "frames_echoed": 0}


class Harness:
    def __init__(self, dut):
        self.dut = dut
        self.cycle = 0
        self.out_frames = []      # (bytes, bad, first_cycle)
        self.errors = []
        self.in_first = []        # input cycle of each offered frame's first beat

    async def start(self):
        d = self.dut
        cocotb.start_soon(Clock(d.clk, 6.4, units="ns").start())
        d.rst.value = 1
        d.s_tvalid.value = 0
        d.s_tlast.value = 0
        d.s_tuser.value = 0
        d.s_tkeep.value = 0
        d.s_tdata.value = 0
        for _ in range(10):
            await RisingEdge(d.clk)
        d.rst.value = 0
        for _ in range(5):
            await RisingEdge(d.clk)
        cocotb.start_soon(self._monitor())

    async def _monitor(self):
        d = self.dut
        buf, bad, first, in_frame = bytearray(), 0, None, False
        while True:
            await RisingEdge(d.clk)
            await ReadOnly()
            self.cycle = now_ps() // PERIOD_PS
            v = d.m_tvalid.value
            if not v.is_resolvable:
                self.errors.append(f"cycle {self.cycle}: m_tvalid is X/Z")
                continue
            if int(v) == 0:
                if in_frame:
                    self.errors.append(f"cycle {self.cycle}: idle cycle inside an output frame "
                                       f"(the TX MAC would abort it)")
                    in_frame = False
                    buf, first = bytearray(), None
                continue
            if not d.m_tdata.value.is_resolvable or not d.m_tkeep.value.is_resolvable:
                self.errors.append(f"cycle {self.cycle}: m_tdata/m_tkeep has X/Z while m_tvalid=1")
                continue
            data = int(d.m_tdata.value)
            keep = int(d.m_tkeep.value)
            last = int(d.m_tlast.value)
            if first is None:
                first = self.cycle
            in_frame = True
            if not last and keep != 0xFF:
                self.errors.append(f"cycle {self.cycle}: m_tkeep 0x{keep:02x} on a beat that is not the last")
            if last and keep not in (0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F, 0xFF):
                self.errors.append(f"cycle {self.cycle}: m_tkeep 0x{keep:02x} is not a contiguous low mask")
            for b in range(8):
                if keep >> b & 1:
                    buf.append(data >> (8 * b) & 0xFF)
            if last:
                bad = int(d.m_tuser.value) if d.m_tuser.value.is_resolvable else 1
                self.out_frames.append((bytes(buf), bad, first))
                buf, first, in_frame = bytearray(), None, False

    async def send(self, schedule):
        """schedule: list of (frame_bytes, bad, idle_cycles_after)."""
        d = self.dut
        for frame, bad, gap in schedule:
            nbeats = (len(frame) + 7) // 8
            for i in range(nbeats):
                await RisingEdge(d.clk)
                chunk = frame[8 * i:8 * i + 8]
                d.s_tdata.value = int.from_bytes(chunk.ljust(8, b"\x00"), "little")
                d.s_tkeep.value = (1 << len(chunk)) - 1
                d.s_tvalid.value = 1
                d.s_tlast.value = int(i == nbeats - 1)
                d.s_tuser.value = int(bad and i == nbeats - 1)
                if i == 0:
                    self.in_first.append(now_ps() // PERIOD_PS)
            for _ in range(gap):
                await RisingEdge(d.clk)
                d.s_tvalid.value = 0
                d.s_tlast.value = 0
                d.s_tuser.value = 0
        await RisingEdge(d.clk)
        d.s_tvalid.value = 0
        d.s_tlast.value = 0
        d.s_tuser.value = 0

    async def drain(self, cycles=2000):
        for _ in range(cycles):
            await RisingEdge(self.dut.clk)


async def run_case(dut, name, schedule):
    """Send the schedule, then check every output frame against the model."""
    h = Harness(dut)
    await h.start()
    await h.send(schedule)
    await h.drain()

    expected = []   # (index into schedule, echoed bytes, bad)
    for idx, (frame, bad, _) in enumerate(schedule):
        if model.must_echo(frame):
            expected.append((idx, model.echo(frame), bad))

    errors = list(h.errors)
    outs = list(h.out_frames)
    lat = []
    ei = 0
    for k, (data, out_bad, first) in enumerate(outs):
        # bad frames may be dropped: skip expected bad frames that have no matching output
        while ei < len(expected) and expected[ei][2] and expected[ei][1] != data:
            ei += 1
        if ei >= len(expected):
            errors.append(f"output frame {k} ({len(data)} B) was not expected (a frame that must be dropped, "
                          f"or a duplicate)")
            continue
        idx, want, want_bad = expected[ei]
        if data != want:
            diff = next((i for i in range(min(len(data), len(want))) if data[i] != want[i]),
                        min(len(data), len(want)))
            errors.append(f"output frame {k} (input frame {idx}, {len(want)} B): first difference at byte {diff}"
                          f" (got {len(data)} B)")
        elif not model.checksums_ok(data):
            errors.append(f"output frame {k}: IP/UDP checksum invalid after the echo")
        if out_bad != want_bad:
            errors.append(f"output frame {k} (input frame {idx}): m_tuser {out_bad}, expected {want_bad}")
        if not want_bad:
            lat.append(first - h.in_first[idx])
        ei += 1
    missing = [e for e in expected[ei:] if not e[2]]
    for idx, want, _ in missing:
        errors.append(f"input frame {idx} ({len(want)} B) must be echoed but never came out")

    stats = {"offered": len(schedule), "echoed": len(outs), "expected": len(expected),
             "latency_min": min(lat) if lat else None, "latency_max": max(lat) if lat else None,
             "errors": len(errors)}
    SUMMARY["tests"][name] = stats
    if lat:
        SUMMARY["frames_echoed"] += len(lat)
        lat = [int(x) for x in lat]
        SUMMARY["latency_max"] = max(lat + ([SUMMARY["latency_max"]] if SUMMARY["latency_max"] is not None else []))
        SUMMARY["latency_min"] = min(lat + ([SUMMARY["latency_min"]] if SUMMARY["latency_min"] is not None else []))
    SUMMARY["pass"] = all(t["errors"] == 0 for t in SUMMARY["tests"].values())
    with open(RESULTS, "w") as fh:
        json.dump(SUMMARY, fh, indent=2)

    dut._log.info(f"{name}: offered {len(schedule)}, echoed {len(outs)} of {len(expected)} expected, "
                  f"latency {stats['latency_min']}..{stats['latency_max']} cycles, {len(errors)} errors")
    for e in errors[:15]:
        dut._log.error(e)
    assert not errors, f"{name}: {len(errors)} errors"


@cocotb.test()
async def test_01_sizes(dut):
    """Every UDP payload size 18..1472, spaced 3 idle cycles apart."""
    rng = random.Random(SEED * 1000 + 1)
    await run_case(dut, "sizes", [(model.build(rng, p), 0, 3) for p in range(18, 1473)])


@cocotb.test()
async def test_02_back_to_back(dut):
    """Frames with no idle cycle between them, short and long mixed."""
    rng = random.Random(SEED * 1000 + 2)
    sizes = [18, 22, 100, 1472, 18, 18, 700, 18, 1472, 64]
    await run_case(dut, "back_to_back", [(model.build(rng, p), 0, 0) for _ in range(12) for p in sizes])


@cocotb.test()
async def test_03_line_rate(dut):
    """Minimum-size frames (60 B) with no gap, faster than 10G line rate."""
    rng = random.Random(SEED * 1000 + 3)
    await run_case(dut, "line_rate", [(model.build(rng, 18), 0, 0) for _ in range(500 * SCALE)])


@cocotb.test()
async def test_04_drops(dut):
    """Non-matching frames between good ones, back to back: only the good ones come out."""
    rng = random.Random(SEED * 1000 + 4)
    sched = []
    for _ in range(3):
        for _, f in model.non_matching(rng, rng.randrange(18, 300)):
            sched.append((model.build(rng, rng.randrange(18, 300)), 0, 0))
            sched.append((f, 0, 0))
    sched.append((model.build(rng, 40), 0, 0))
    await run_case(dut, "drops", sched)


@cocotb.test()
async def test_05_bad_frames(dut):
    """Frames flagged bad (TUSER on the last beat): drop them, or echo them with m_tuser set."""
    rng = random.Random(SEED * 1000 + 5)
    await run_case(dut, "bad_frames", [(model.build(rng, 30 + 97 * i), i % 2, rng.randrange(0, 4))
                                       for i in range(24)])


@cocotb.test()
async def test_06_random(dut):
    """Random sizes, gaps, non-matching and bad frames mixed."""
    rng = random.Random(SEED * 1000 + 6)
    kinds = list(model.non_matching(rng))
    sched = []
    for _ in range(400 * SCALE):
        r = rng.random()
        if r < 0.15:
            frame = rng.choice(kinds)[1]
        else:
            frame = model.build(rng, rng.randrange(18, 1473))
        bad = 1 if rng.random() < 0.05 else 0
        sched.append((frame, bad, rng.choice([0, 0, 0, 1, 2, 3, 8])))
    await run_case(dut, "random", sched)
