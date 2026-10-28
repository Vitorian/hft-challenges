# Setting up the AMD tools for the FPGA challenges

**You only need this for HLS builds (`build.sh`) and local timing runs (`timing.sh`).** The tests
(`test.sh`) run in a container with free simulators and need no AMD tools; the grader does the timing
run for every submission anyway.

The grader uses **Vivado and Vitis HLS 2026.1**. Other versions place and route differently (timing
slack can move by a fraction of a nanosecond) and, for the HLS lane, generate different RTL from the
same C++ (latency can change by a cycle). The grader's results are the official ones; use 2026.1 to
see the same numbers locally.

## 1. Install (Linux)

1. Create a free AMD account and download the **2026.1 unified installer** for Linux from AMD's
   Vivado download page.
2. Select **Vivado**, plus **Vitis** for the HLS lane (Vitis HLS is part of it).
3. Under devices, select only **Virtex UltraScale+ HBM** (the Alveo U50 family). The full install
   with every device family is over 100 GB; one family is much smaller.
4. Run the dependency script it ships, `Vitis/scripts/installLibs.sh`, with sudo. On some versions
   that file has Windows line endings and fails with "No such file or directory"; strip them first:

   ```bash
   tr -d '\r' < Vitis/scripts/installLibs.sh > /tmp/installLibs.sh && sudo bash /tmp/installLibs.sh
   ```

   On Ubuntu 24.04, `libasound2` is called `libasound2t64`.

Windows: Vivado and Vitis HLS run natively, but the scripts here are bash (WSL2 is untested).
macOS: there is no Vivado for macOS; you can do everything in the RTL lane except local timing.

## 2. Licence

Vivado 2026.1 needs a licence file even for the free tier that targets Alveo cards.

1. In your AMD account, open the licence form (Product Licensing). If the usual
   `xilinx.com/getlicense` link does not work, the "Evaluate" button on any AMD IP product page leads
   to the same form.
2. Create a **node-locked** licence with the free entries for **Vivado (Alveo edition)** and
   **Vitis HLS**.
3. The host ID is the MAC address of one of your machine's network interfaces
   (`ip link`, without the colons).
4. Save the file as `~/.Xilinx/Xilinx.lic`, or point `XILINXD_LICENSE_FILE` at it.

## 3. Use

```bash
source /opt/Xilinx/2026.1/Vitis/settings64.sh     # for ./build.sh  (HLS lane)
source /opt/Xilinx/2026.1/Vivado/settings64.sh    # for ./timing.sh (both lanes)
vivado -version     # vivado v2026.1 (64-bit)
v++ --version       # v++ v2026.1 (64-bit)
```

Adjust the path to wherever you installed. `build.sh` and `timing.sh` print a warning if they find
another version.
