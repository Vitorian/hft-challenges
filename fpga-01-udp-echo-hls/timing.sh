#!/usr/bin/env bash
# Out-of-context timing of build/rtl at 156.25 MHz on the U50's part (needs Vivado). Writes timing/summary.json.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
[ -f "$here/build/rtl/udp_echo.v" ] || { echo "run ./build.sh first"; exit 1; }
command -v vivado >/dev/null || { echo "vivado not found: source <install>/2026.1/Vivado/settings64.sh (see ../common/fpga/SETUP.md)"; exit 1; }
ver="$(vivado -version 2>/dev/null | head -1)"
case "$ver" in
    *v2026.1*) ;;
    *) echo "WARNING: the grader uses Vivado 2026.1; found '${ver:-unknown}'. Timing can differ (see ../common/fpga/SETUP.md)." ;;
esac
mkdir -p "$here/timing"
cp "$here"/build/rtl/*.dat "$here/timing/" 2>/dev/null || true
cd "$here/timing"
vivado -mode batch -nojournal -log vivado.log -source "$here/../common/fpga/fpga01/timing/timing.tcl" \
    -tclargs "$here/build/rtl" "$here/timing"
