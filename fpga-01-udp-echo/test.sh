#!/usr/bin/env bash
# Run the FPGA 01 tests on solution/ in the simulation container (podman or docker).
#   ./test.sh                 # Icarus, public seed
#   ./test.sh verilator       # faster
#   ./test.sh icarus 7        # another random stream
# Writes results/results.json (pass, per-group counts, worst latency in cycles).
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
common="$here/../common/fpga"
sim="${1:-icarus}"
seed="${2:-1}"
engine="$(command -v podman || command -v docker || true)"
[ -n "$engine" ] || { echo "needs podman or docker"; exit 1; }
image="hftu-fpga-sim:1"
"$engine" image inspect "$image" >/dev/null 2>&1 || "$engine" build -t "$image" "$common/docker"
mkdir -p "$here/results"
"$engine" run --rm -v "$common/fpga01/tb":/tb:ro -v "$here/solution":/dut:ro -v "$here/results":/out "$image" \
    sh -c "cp -r /tb /tmp/tb && cd /tmp/tb && make DUT=/dut SIM=$sim FPGA01_SEED=$seed FPGA01_RESULTS=/out/results.json"
