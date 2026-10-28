# FPGA 02 cocotb testbench.
#   make DUT=<dir with your .v/.sv files> W=64                # Icarus (default)
#   make DUT=<dir> W=32 SIM=verilator
#   make DUT=<dir> W=64 FPGA02_SEED=7 FPGA02_SCALE=4
# Top-level module: fpga02_rx with parameter W. Run `make clean` when switching DUT or W.
DUT ?= ../../../fpga-02-10g-rx/solution
W ?= 64
SIM ?= icarus
TOPLEVEL_LANG = verilog
VERILOG_SOURCES = $(wildcard $(abspath $(DUT))/*.v) $(wildcard $(abspath $(DUT))/*.sv)
TOPLEVEL = fpga02_rx
MODULE = test_fpga02
COCOTB_HDL_TIMEUNIT = 1ns
COCOTB_HDL_TIMEPRECISION = 1ps
ifeq ($(SIM),icarus)
COMPILE_ARGS += -g2012 -Pfpga02_rx.W=$(W)
endif
ifeq ($(SIM),verilator)
EXTRA_ARGS += --no-timing -Wno-fatal -GW=$(W)
endif
FPGA02_W := $(W)
export FPGA02_W FPGA02_SEED FPGA02_SCALE FPGA02_RESULTS
include $(shell cocotb-config --makefiles)/Makefile.sim
