set projDir "/home/ozy/alchitry/au_test/work/vivado"
set projName "au_test"
set topName top
set device xc7a35tftg256-1
if {[file exists "$projDir/$projName"]} { file delete -force "$projDir/$projName" }
create_project $projName "$projDir/$projName" -part $device
set_property design_mode RTL [get_filesets sources_1]
set verilogSources [list "/home/ozy/alchitry/au_test/work/verilog/au_top_0.v" "/home/ozy/alchitry/au_test/work/verilog/reset_conditioner_1.v" ]
import_files -fileset [get_filesets sources_1] -force -norecurse $verilogSources
set xdcSources [list "/home/ozy/Downloads/alchitry-labs-1.2.7-linux/alchitry-labs-1.2.7/library/components/au.xdc" "/home/ozy/alchitry/au_test/work/constraint/alchitry.xdc" ]
read_xdc $xdcSources
set_property STEPS.WRITE_BITSTREAM.ARGS.BIN_FILE true [get_runs impl_1]
update_compile_order -fileset sources_1
launch_runs -runs synth_1 -jobs 16
wait_on_run synth_1
launch_runs impl_1 -to_step write_bitstream -jobs 16
wait_on_run impl_1
