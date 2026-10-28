#!/usr/bin/env bash
# Out-of-context timing of solution/ at 322.27 MHz (W=32) or 161.13 MHz (W=64) on the U50's part
# (needs Vivado 2026.1 on the PATH; see ../common/fpga/SETUP.md). The grader runs the same script.
#   ./timing.sh 64     # writes timing/summary_W64.json
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
w="${1:-64}"
case "$w" in 32|64) ;; *) echo "width must be 32 or 64"; exit 1 ;; esac
command -v vivado >/dev/null || { echo "vivado not found: source <install>/2026.1/Vivado/settings64.sh (see ../common/fpga/SETUP.md)"; exit 1; }
ver="$(vivado -version 2>/dev/null | head -1)"
case "$ver" in
    *v2026.1*) ;;
    *) echo "WARNING: the grader uses Vivado 2026.1; found '${ver:-unknown}'. Timing can differ (see ../common/fpga/SETUP.md)." ;;
esac
mkdir -p "$here/timing"
cd "$here/timing"
vivado -mode batch -nojournal -log "vivado_W$w.log" -source "$here/../common/fpga/fpga02/timing/timing.tcl" \
    -tclargs "$w" "$here/solution" "$here/timing"
