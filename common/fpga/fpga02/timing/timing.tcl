# FPGA 02 timing check (Vivado, out of context):
#   vivado -mode batch -source timing.tcl -tclargs <W 32|64> <dir with the design's .v/.sv> <out dir> [part]
# Pass = WNS >= 0 and WHS >= 0 at 322.265625 MHz (W=32) or 161.1328125 MHz (W=64).
set W    [lindex $argv 0]
set src  [file normalize [lindex $argv 1]]
set out  [file normalize [lindex $argv 2]]
set part [expr {[llength $argv] > 3 ? [lindex $argv 3] : "xcu50-fsvh2104-2-e"}]
set here [file dirname [file normalize [info script]]]
set period [expr {$W == 32 ? 3.103 : 6.206}]
file mkdir $out
foreach f [glob -nocomplain $src/*.v $src/*.sv] { read_verilog -sv $f }
read_verilog $here/fpga02_timing_top.v
synth_design -mode out_of_context -top fpga02_timing_top -part $part -generic W=$W -flatten_hierarchy rebuilt
create_clock -name clk -period $period [get_ports clk]
opt_design
place_design
phys_opt_design
route_design
report_timing_summary -max_paths 5 -file $out/timing_W$W.rpt
report_utilization -file $out/util_W$W.rpt
set wns [get_property SLACK [get_timing_paths -max_paths 1 -nworst 1 -setup]]
set whs [get_property SLACK [get_timing_paths -max_paths 1 -nworst 1 -hold]]
set lut [llength [get_cells -hier -filter {REF_NAME =~ LUT*}]]
set ff  [llength [get_cells -hier -filter {REF_NAME =~ FD*}]]
set ram [llength [get_cells -hier -filter {REF_NAME =~ RAMB*}]]
set fh [open $out/summary_W$W.json w]
puts $fh "{\"W\": $W, \"period_ns\": $period, \"part\": \"$part\", \"wns\": $wns, \"whs\": $whs, \"pass\": [expr {$wns >= 0 && $whs >= 0 ? "true" : "false"}], \"luts\": $lut, \"ffs\": $ff, \"brams\": $ram}"
close $fh
puts "FPGA02_TIMING W=$W period=$period WNS=$wns WHS=$whs LUT=$lut FF=$ff BRAM=$ram"
