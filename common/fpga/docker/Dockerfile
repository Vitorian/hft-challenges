# FPGA 01 participant/grader simulation image: cocotb + Icarus Verilog + Verilator.
# No AMD tools inside (their licence does not allow redistribution); the HLS lane needs
# Vitis HLS installed on the host to turn C++ into RTL, then this image simulates the RTL.
# One Python only (Debian's): Verilator's build calls /usr/bin/python3, so a second Python
# (e.g. the python:3.x images) breaks SIM=verilator.
FROM docker.io/library/debian:bookworm-slim
RUN apt-get update \
 && apt-get install -y --no-install-recommends python3 python3-venv python3-dev iverilog verilator make g++ \
 && rm -rf /var/lib/apt/lists/*
RUN python3 -m venv /opt/venv && /opt/venv/bin/pip install --no-cache-dir cocotb==1.9.2 pytest==8.3.3
ENV PATH=/opt/venv/bin:$PATH
WORKDIR /work
