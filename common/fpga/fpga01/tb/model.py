"""FPGA 01 reference model: frame builder and the golden echo.

A frame here is what the 10G MAC hands over: Ethernet II header onwards, no preamble, no FCS.
"""
import random
import struct

ECHO_IP = bytes([192, 168, 1, 128])
ECHO_PORT = 1234


def ip_checksum(hdr: bytes) -> int:
    s = sum(struct.unpack(f"!{len(hdr) // 2}H", hdr))
    while s >> 16:
        s = (s & 0xFFFF) + (s >> 16)
    return ~s & 0xFFFF


def udp_checksum(src_ip: bytes, dst_ip: bytes, udp: bytes) -> int:
    pseudo = src_ip + dst_ip + struct.pack("!BBH", 0, 17, len(udp))
    data = pseudo + udp + (b"\x00" if len(udp) % 2 else b"")
    c = ip_checksum(data)
    return c if c != 0 else 0xFFFF


def build(rng: random.Random, payload_len: int, *, dst_ip=ECHO_IP, dport=ECHO_PORT, proto=17,
          ethertype=0x0800, ver_ihl=0x45, frag=0x4000) -> bytes:
    """A UDP/IPv4 frame with valid checksums. Defaults make a frame the echo must answer."""
    dst_mac = bytes([0x02, 0, 0, 0, 0, 0])
    src_mac = bytes([0x02, 0, 0, 0, rng.randrange(256), rng.randrange(256)])
    src_ip = bytes([192, 168, 1, rng.randrange(1, 127)])
    sport = rng.randrange(1024, 65536)
    payload = bytes(rng.randrange(256) for _ in range(payload_len))
    udp = struct.pack("!HHHH", sport, dport, 8 + payload_len, 0) + payload
    if proto == 17:
        udp = udp[:6] + struct.pack("!H", udp_checksum(src_ip, dst_ip, udp)) + udp[8:]
    ip = struct.pack("!BBHHHBBH4s4s", ver_ihl, 0, 20 + len(udp), rng.randrange(65536), frag, 64, proto, 0,
                     src_ip, dst_ip)
    ip = ip[:10] + struct.pack("!H", ip_checksum(ip)) + ip[12:]
    frame = dst_mac + src_mac + struct.pack("!H", ethertype) + ip + udp
    return frame


def must_echo(f: bytes) -> bool:
    """The challenge rule: echo IPv4 (no options) / UDP, unfragmented, to ECHO_IP:ECHO_PORT."""
    if len(f) < 42:
        return False
    return (f[12:14] == b"\x08\x00" and f[14] == 0x45 and f[23] == 17
            and (struct.unpack("!H", f[20:22])[0] & 0x3FFF) == 0
            and f[30:34] == ECHO_IP and struct.unpack("!H", f[36:38])[0] == ECHO_PORT)


def echo(f: bytes) -> bytes:
    """Swap Ethernet MACs, IPv4 addresses and UDP ports; nothing else changes (checksums stay valid)."""
    o = bytearray(f)
    o[0:6], o[6:12] = f[6:12], f[0:6]
    o[26:30], o[30:34] = f[30:34], f[26:30]
    o[34:36], o[36:38] = f[36:38], f[34:36]
    return bytes(o)


def checksums_ok(f: bytes) -> bool:
    ip = f[14:34]
    if ip_checksum(ip) != 0:
        return False
    udp = f[34:]
    if struct.unpack("!H", udp[6:8])[0] == 0:
        return True
    pseudo = f[26:30] + f[30:34] + struct.pack("!BBH", 0, 17, len(udp))
    data = pseudo + udp + (b"\x00" if len(udp) % 2 else b"")
    return ip_checksum(data) == 0


def non_matching(rng: random.Random, payload_len: int = 40):
    """Frames the echo must drop, one of each kind."""
    yield "wrong port", build(rng, payload_len, dport=ECHO_PORT + 1)
    yield "wrong IP", build(rng, payload_len, dst_ip=bytes([192, 168, 1, 129]))
    yield "TCP", build(rng, payload_len, proto=6)
    yield "ARP ethertype", build(rng, payload_len, ethertype=0x0806)
    yield "VLAN tag", build(rng, payload_len, ethertype=0x8100)
    yield "IPv6 ethertype", build(rng, payload_len, ethertype=0x86DD)
    yield "IP options (IHL 6)", build(rng, payload_len, ver_ihl=0x46)
    yield "more fragments", build(rng, payload_len, frag=0x2000)
    yield "fragment offset", build(rng, payload_len, frag=0x00B9)
    yield "runt 40 B", bytes(rng.randrange(256) for _ in range(40))
    yield "runt 20 B", bytes(rng.randrange(256) for _ in range(20))
