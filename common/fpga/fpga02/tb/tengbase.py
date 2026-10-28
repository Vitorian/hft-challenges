"""10GBASE-R reference model for FPGA 02 (IEEE 802.3 clause 46 XGMII + clause 49 PCS).

Transmitter: frames -> XGMII (preamble, SFD, FCS, idles, start in lane 0 or 4) -> 64b/66b
blocks -> self-synchronizing scrambler (x^58 + x^39 + 1) -> line bits, with error injection.
Receiver: line bits from any offset -> block lock -> descrambler -> decoder -> XGMII -> frames,
FCS check. The two halves are written independently so each checks the other.

Conventions (as in Corundum's verilog-ethernet, whose encoder interoperates with real NICs):
- a block is (hdr, payload): hdr 0b10 = data, 0b01 = control, value LSB = first bit on the wire;
  payload bit 0 goes first, bits 7:0 = block type of a control block, lane n = bits 8n+7:8n
- line bit order: hdr bit 0, hdr bit 1, payload bit 0 ... payload bit 63
- XGMII words: 8 (byte, is_ctrl) lanes, lane 0 first
"""
import random
import struct
import zlib

# XGMII control characters
IDLE, START, TERM, ERROR, SEQ_OS, LPI = 0x07, 0xFB, 0xFD, 0xFE, 0x9C, 0x06
PREAMBLE = [0x55] * 6 + [0xD5]          # after /S/, which takes the first preamble byte

# 7-bit control codes in 64b/66b blocks
CTRL_CODE = {IDLE: 0x00, LPI: 0x06, ERROR: 0x1E}
CODE_CTRL = {v: k for k, v in CTRL_CODE.items()}
O_SEQ_OS = 0x0

SYNC_DATA, SYNC_CTRL = 0b10, 0b01

BT_CTRL, BT_OS_4, BT_START_4, BT_OS_START, BT_OS_04, BT_START_0, BT_OS_0 = 0x1E, 0x2D, 0x33, 0x66, 0x55, 0x78, 0x4B
BT_TERM = [0x87, 0x99, 0xAA, 0xB4, 0xCC, 0xD2, 0xE1, 0xFF]
MASK58 = (1 << 58) - 1


def fcs(frame: bytes) -> bytes:
    return struct.pack("<I", zlib.crc32(frame) & 0xFFFFFFFF)


# ---------------------------------------------------------------------------------------------
# Transmitter
# ---------------------------------------------------------------------------------------------

class TxFrame:
    """One frame to send. ipg = idle bytes before it (after the previous /T/), before lane alignment."""

    def __init__(self, data: bytes, ipg: int = 12, bad_fcs: bool = False, error_at: int = None):
        self.data = data
        self.ipg = ipg
        self.bad_fcs = bad_fcs
        self.error_at = error_at      # replace frame byte error_at with /E/ (XGMII error)
        self.byte0_bit = None         # filled in: line bit index of the first bit of frame byte 0
        self.lane_start = None        # 0 or 4


def mac_tx(frames, lead_idles=8 * 400, tail_idles=24, os_every=0, rng=None):
    """Frames -> list of XGMII words [(byte, ctrl) * 8]. Records where each frame starts.

    lead_idles: idle bytes before the first frame. Block lock slips one bit per invalid header and
    needs 64 valid headers in a row; the search over 66 alignments plus the 64 takes ~130 blocks
    in this model and ~250 in hardware that skips the block in flight during a slip, so the
    default is 400 blocks (a real link idles long before traffic).
    os_every: if > 0, put a sequence ordered set (local fault pattern) into roughly one in
    os_every gaps, to check that receivers ignore it.
    """
    lanes = []                    # flat (byte, ctrl) stream
    frame_lane0 = []              # index in `lanes` of frame byte 0, per frame

    def idle(n):
        lanes.extend([(IDLE, True)] * n)

    idle(lead_idles)
    for k, f in enumerate(frames):
        idle(f.ipg)
        if os_every and rng is not None and rng.randrange(os_every) == 0:
            while len(lanes) % 8:
                idle(1)
            lanes.extend([(SEQ_OS, True), (0x00, False), (0x00, False), (0x01, False)] + [(IDLE, True)] * 4)
        while len(lanes) % 4:
            idle(1)
        f.lane_start = len(lanes) % 8
        lanes.append((START, True))
        lanes.extend((b, False) for b in PREAMBLE)
        frame_lane0.append(len(lanes))
        body = f.data + (bytes([b ^ 0xFF for b in fcs(f.data)]) if f.bad_fcs else fcs(f.data))
        for i, b in enumerate(body):
            lanes.append((ERROR, True) if f.error_at == i else (b, False))
        lanes.append((TERM, True))
    idle(tail_idles)
    while len(lanes) % 8:
        idle(1)
    words = [lanes[i:i + 8] for i in range(0, len(lanes), 8)]
    return words, frame_lane0


def encode(word):
    """One XGMII word -> (hdr, payload), as Corundum's xgmii_baser_enc_64."""
    d = [b for b, _ in word]
    c = [ct for _, ct in word]
    if not any(c):
        return SYNC_DATA, int.from_bytes(bytes(d), "little")

    def cc(i):
        return CTRL_CODE.get(d[i], CTRL_CODE[ERROR]) if c[i] else None

    def ctrl_bits(lo, hi):          # 7-bit codes of lanes lo..hi-1, lane lo in the low bits
        v = 0
        for n, i in enumerate(range(lo, hi)):
            v |= (cc(i) if cc(i) is not None else CTRL_CODE[ERROR]) << (7 * n)
        return v

    def data_bits(lo, hi):
        return int.from_bytes(bytes(d[lo:hi]), "little")

    cmask = sum(1 << i for i in range(8) if c[i])
    if cmask == 0x01 and d[0] == START:
        p = BT_START_0 | data_bits(1, 8) << 8
    elif cmask == 0x1F and d[4] == START:
        p = BT_START_4 | ctrl_bits(0, 4) << 8 | 0 << 36 | data_bits(5, 8) << 40
    elif cmask == 0xF1 and d[0] == SEQ_OS:
        p = BT_OS_0 | data_bits(1, 4) << 8 | O_SEQ_OS << 32 | ctrl_bits(4, 8) << 36
    elif cmask == 0x11 and d[0] == SEQ_OS and d[4] == START:
        p = BT_OS_START | data_bits(1, 4) << 8 | O_SEQ_OS << 32 | 0 << 36 | data_bits(5, 8) << 40
    elif cmask == 0x1F and d[4] == SEQ_OS:
        p = BT_OS_4 | ctrl_bits(0, 4) << 8 | O_SEQ_OS << 36 | data_bits(5, 8) << 40
    elif cmask == 0xFF and all(d[i] in CTRL_CODE for i in range(8)):
        p = BT_CTRL | ctrl_bits(0, 8) << 8
    else:
        for k in range(8):
            if cmask == (0xFF << k) & 0xFF and d[k] == TERM:
                p = BT_TERM[k] | data_bits(0, k) << 8
                if k < 7:
                    p |= ctrl_bits(k + 1, 8) << (8 + 8 * k + (7 - k))
                break
        else:
            p = BT_CTRL | sum(CTRL_CODE[ERROR] << (8 + 7 * i) for i in range(8))
    return SYNC_CTRL, p


def scramble(payload: int, state: int):
    out = 0
    for i in range(64):
        s = (payload >> i & 1) ^ (state >> 38 & 1) ^ (state >> 57 & 1)
        state = ((state << 1) | s) & MASK58
        out |= s << i
    return out, state


def descramble(payload: int, state: int):
    out = 0
    for i in range(64):
        s = payload >> i & 1
        out |= (s ^ (state >> 38 & 1) ^ (state >> 57 & 1)) << i
        state = ((state << 1) | s) & MASK58
    return out, state


def phy_tx(words, scrambler_state=MASK58):
    """XGMII words -> list of scrambled (hdr, payload) blocks."""
    blocks = []
    st = scrambler_state
    for w in words:
        h, p = encode(w)
        sp, st = scramble(p, st)
        blocks.append((h, sp))
    return blocks


def serialize(blocks):
    bits = []
    for h, p in blocks:
        bits.append(h & 1)
        bits.append(h >> 1 & 1)
        bits.extend(p >> i & 1 for i in range(64))
    return bits


def transmit(frames, rng: random.Random, offset=None, os_every=0):
    """Frames -> (line bits, frames with byte0_bit filled). A random number of junk bits (the link
    partner was already running) precede the first block, so the receiver must find alignment."""
    words, lane0 = mac_tx(frames, os_every=os_every, rng=rng)
    blocks = phy_tx(words, scrambler_state=rng.getrandbits(58))
    bits = serialize(blocks)
    if offset is None:
        offset = rng.randrange(66 * 4)
    pre = [rng.getrandbits(1) for _ in range(offset)]
    for f, li in zip(frames, lane0):
        blk, lane = divmod(li, 8)
        f.byte0_bit = offset + 66 * blk + 2 + 8 * lane
    return pre + bits, frames


def flip_bits(bits, positions):
    b = list(bits)
    for p in positions:
        b[p] ^= 1
    return b


# ---------------------------------------------------------------------------------------------
# Receiver
# ---------------------------------------------------------------------------------------------

def decode(hdr, payload):
    """(hdr, descrambled payload) -> XGMII word, as Corundum's xgmii_baser_dec_64. Invalid -> all /E/."""
    err = [(ERROR, True)] * 8
    if hdr == SYNC_DATA:
        return [(payload >> (8 * i) & 0xFF, False) for i in range(8)]
    if hdr != SYNC_CTRL:
        return err
    bt = payload & 0xFF

    def code(bitpos):
        v = payload >> bitpos & 0x7F
        return (CODE_CTRL[v], True) if v in CODE_CTRL else None

    def data(lane_from_bit):
        return (payload >> lane_from_bit & 0xFF, False)

    if bt == BT_CTRL:
        out = [code(8 + 7 * i) for i in range(8)]
        return err if None in out else out
    if bt == BT_START_0:
        return [(START, True)] + [data(8 * i) for i in range(1, 8)]
    if bt == BT_START_4:
        out = [code(8 + 7 * i) for i in range(4)]
        if None in out:
            return err
        return out + [(START, True)] + [data(40 + 8 * i) for i in range(3)]
    if bt == BT_OS_0:
        tail = [code(36 + 7 * i) for i in range(4)]
        if None in tail:
            return err
        return [(SEQ_OS, True)] + [data(8 + 8 * i) for i in range(3)] + tail
    if bt == BT_OS_START:
        return [(SEQ_OS, True)] + [data(8 + 8 * i) for i in range(3)] + [(START, True)] + [data(40 + 8 * i) for i in range(3)]
    if bt == BT_OS_4:
        head = [code(8 + 7 * i) for i in range(4)]
        if None in head:
            return err
        return head + [(SEQ_OS, True)] + [data(40 + 8 * i) for i in range(3)]
    if bt in BT_TERM:
        k = BT_TERM.index(bt)
        out = [data(8 + 8 * i) for i in range(k)] + [(TERM, True)]
        if k < 7:
            tail = [code(8 + 8 * k + (7 - k) + 7 * i) for i in range(7 - k)]
            if None in tail:
                return err
            out += tail
        return out
    return err


class RxModel:
    """Line bits -> frames. Clause 49 block lock: 64 consecutive valid sync headers to lock; while
    locked, 16 invalid headers within a window of 64 drop lock; slip one bit on loss."""

    def __init__(self):
        self.frames = []          # (bytes without FCS, bad, byte0_bit)
        self.lock_bit = None      # line bit index where block lock was first reached
        self.lock_losses = 0

    def run(self, bits):
        pos = 0
        good = 0
        locked = False
        win = bad_in_win = 0
        state = 0
        words = []                # (xgmii word, bit index of payload bit 0)
        while pos + 66 <= len(bits):
            h = bits[pos] | bits[pos + 1] << 1
            valid = h in (SYNC_DATA, SYNC_CTRL)
            if not locked:
                if valid:
                    good += 1
                    if good >= 64:
                        locked = True
                        win = bad_in_win = 0
                        if self.lock_bit is None:
                            self.lock_bit = pos + 66
                else:
                    good = 0
                    pos += 1                    # slip
                    continue
            else:
                win += 1
                if not valid:
                    bad_in_win += 1
                if bad_in_win >= 16:
                    locked = False
                    good = 0
                    self.lock_losses += 1
                    pos += 1
                    continue
                if win == 64:
                    win = bad_in_win = 0
            p = sum(bits[pos + 2 + i] << i for i in range(64))
            dp = 0
            for i in range(64):
                s = p >> i & 1
                dp |= (s ^ (state >> 38 & 1) ^ (state >> 57 & 1)) << i
                state = ((state << 1) | s) & MASK58
            if locked:
                words.append((decode(h, dp), pos + 2))
            pos += 66
        self._mac(words)
        return self.frames

    def _mac(self, words):
        lanes = []
        for w, b0 in words:
            for i, lc in enumerate(w):
                lanes.append((lc, b0 + 8 * i))
        i = 0
        n = len(lanes)
        while i < n:
            (b, c), _ = lanes[i]
            if not (c and b == START and i % 4 == 0):
                i += 1
                continue
            j = i + 1
            pre = [lanes[j + k][0] for k in range(7)] if j + 7 <= n else []
            bad = pre != [(x, False) for x in PREAMBLE]
            j += 7
            body = []
            byte0 = lanes[j][1] if j < n else None
            while j < n:
                (b, c), _ = lanes[j]
                if c:
                    if b != TERM:
                        bad = True
                    break
                body.append(b)
                j += 1
            else:
                break                               # stream ended inside a frame
            if (b, c) != (TERM, True):
                bad = True
            data = bytes(body[:-4]) if len(body) >= 4 else b""
            if len(body) < 4 or fcs(data) != bytes(body[-4:]):
                bad = True
            self.frames.append((data, bad, byte0))
            i = j + 1
