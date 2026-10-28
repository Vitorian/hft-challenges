#!/usr/bin/env bash
# Vitis HLS (2026.1, on the host): solution/udp_echo.cpp -> build/rtl (generated Verilog, its .dat
# RAM-init files, and the fpga01_echo wrapper the tests and the timing run use).
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
cd "$here"
command -v v++ >/dev/null || { echo "v++ not found: source <install>/2026.1/Vitis/settings64.sh (see ../common/fpga/SETUP.md)"; exit 1; }
ver="$(v++ --version 2>/dev/null | grep -m1 -o 'v++ v[0-9.]*')"
case "$ver" in
    *v2026.1*) ;;
    *) echo "WARNING: the grader synthesizes with Vitis HLS 2026.1; found '${ver:-unknown}'. Latency and resources can differ (see ../common/fpga/SETUP.md)." ;;
esac
mkdir -p build/rtl
v++ -c --mode hls --config hls_config.cfg --work_dir build/hls
cp build/hls/hls/syn/verilog/* build/rtl/
cp ../common/fpga/fpga01/hls/fpga01_echo_hls.v build/rtl/
echo "RTL in build/rtl; now ./test.sh"
