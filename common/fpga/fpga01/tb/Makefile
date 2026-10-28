# FPGA 01 cocotb testbench.
#   make DUT=<dir with your .v/.sv files>                  # Icarus (default)
#   make DUT=<dir> SIM=verilator                           # faster
#   make DUT=<dir> FPGA01_SEED=7 FPGA01_SCALE=4            # other random streams
# Top-level module: fpga01_echo (see the challenge README for the ports).
# Run `make clean` when you switch DUT directories: the compiled simulation in sim_build/ is
# reused as long as no source file is newer than it.
DUT ?= ../rtl
SIM ?= icarus
TOPLEVEL_LANG = verilog
VERILOG_SOURCES = $(wildcard $(abspath $(DUT))/*.v) $(wildcard $(abspath $(DUT))/*.sv)
TOPLEVEL = fpga01_echo
MODULE = test_fpga01
COCOTB_HDL_TIMEUNIT = 1ns
COCOTB_HDL_TIMEPRECISION = 1ps
ifeq ($(SIM),icarus)
COMPILE_ARGS += -g2012
endif
ifeq ($(SIM),verilator)
EXTRA_ARGS += --no-timing -Wno-fatal
endif
export FPGA01_SEED FPGA01_SCALE FPGA01_RESULTS
include $(shell cocotb-config --makefiles)/Makefile.sim
