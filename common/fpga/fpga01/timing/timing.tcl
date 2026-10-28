# FPGA 01 timing check (Vivado, out of context):
#   vivado -mode batch -source timing.tcl -tclargs <dir with the design's .v/.sv> <out dir> [part]
# Pass = WNS >= 0 and WHS >= 0 at 156.25 MHz (6.4 ns). For the HLS lane, point it at the generated
# Verilog plus the fpga01_echo wrapper.
set src  [file normalize [lindex $argv 0]]
set out  [file normalize [lindex $argv 1]]
set part [expr {[llength $argv] > 2 ? [lindex $argv 2] : "xcu50-fsvh2104-2-e"}]
set here [file dirname [file normalize [info script]]]
file mkdir $out
foreach f [glob -nocomplain $src/*.v $src/*.sv] { read_verilog -sv $f }
read_verilog $here/fpga01_timing_top.v
synth_design -mode out_of_context -top fpga01_timing_top -part $part -flatten_hierarchy rebuilt
create_clock -name clk -period 6.4 [get_ports clk]
opt_design
place_design
phys_opt_design
route_design
report_timing_summary -max_paths 5 -file $out/timing.rpt
report_utilization -file $out/util.rpt
set wns [get_property SLACK [get_timing_paths -max_paths 1 -nworst 1 -setup]]
set whs [get_property SLACK [get_timing_paths -max_paths 1 -nworst 1 -hold]]
set lut [llength [get_cells -hier -filter {REF_NAME =~ LUT*}]]
set ff  [llength [get_cells -hier -filter {REF_NAME =~ FD*}]]
set ram [llength [get_cells -hier -filter {REF_NAME =~ RAMB*}]]
set fh [open $out/summary.json w]
puts $fh "{\"period_ns\": 6.4, \"part\": \"$part\", \"wns\": $wns, \"whs\": $whs, \"pass\": [expr {$wns >= 0 && $whs >= 0 ? "true" : "false"}], \"luts\": $lut, \"ffs\": $ff, \"brams\": $ram}"
close $fh
puts "FPGA01_TIMING WNS=$wns WHS=$whs LUT=$lut FF=$ff BRAM=$ram"
