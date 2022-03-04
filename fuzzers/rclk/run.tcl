set INDEX $::env(INDEX)
create_project -force -part xczu7ev-ffvf1517-2-e rclk_${INDEX} rclk_${INDEX}
link_design
set_property SEVERITY Warning [get_drc_checks]
opt_design
create_cell -reference LUT6 "misc_lut"
place_design
source gen_design_${INDEX}.tcl
write_checkpoint -force rclk_${INDEX}_preroute.dcp
route_design
# set_property BITSTREAM.GENERAL.PERFRAMECRC YES [current_design]
write_checkpoint -force rclk_${INDEX}.dcp
write_edif -force rclk_${INDEX}.edf
write_bitstream -force rclk_${INDEX}.bit
source $::env(MEOW_UTIL)/tcl/dump_design.tcl
dump_design rclk_${INDEX}.dump

