#!/usr/bin/env bash
# Run the FPGA 02 tests on solution/ in the simulation container (podman or docker).
#   ./test.sh 64              # W=64 (161.13 MHz), Icarus, public seed
#   ./test.sh 32 verilator    # W=32 (322.27 MHz), faster simulator
#   ./test.sh 64 icarus 7     # another random stream
# Writes results/results_W<W>.json (pass, per-group counts, worst latency in cycles and ns).
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
common="$here/../common/fpga"
w="${1:-64}"
sim="${2:-icarus}"
seed="${3:-1}"
case "$w" in 32|64) ;; *) echo "width must be 32 or 64"; exit 1 ;; esac
engine="$(command -v podman || command -v docker || true)"
[ -n "$engine" ] || { echo "needs podman or docker"; exit 1; }
image="hftu-fpga-sim:1"
"$engine" image inspect "$image" >/dev/null 2>&1 || "$engine" build -t "$image" "$common/docker"
mkdir -p "$here/results"
"$engine" run --rm -v "$common/fpga02/tb":/tb:ro -v "$here/solution":/dut:ro -v "$here/results":/out "$image" \
    sh -c "cp -r /tb /tmp/tb && cd /tmp/tb && make DUT=/dut W=$w SIM=$sim FPGA02_SEED=$seed FPGA02_RESULTS=/out/results_W$w.json"
