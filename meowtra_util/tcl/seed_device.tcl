create_project -force -part $::env(MEOW_PART) empty_design empty_design
link_design
# Avoid empty design issues
create_cell -reference LUT6 "misc_lut"
place_design
route_design

set_property BITSTREAM.GENERAL.PERFRAMECRC YES [current_design]
set_property SEVERITY {Warning} [get_drc_checks]

# Write bitstream with PERFRAMECRC and no compression to get frame order
write_bitstream -force $::env(MEOW_WORK)/null.bit

# Write out tile data
set f [open $::env(MEOW_WORK)/raw_tiles.txt w]
foreach t [get_tiles] {
	puts -nonewline $f "[get_property COLUMN $t],[get_property ROW $t],[get_clock_regions -of_objects $t],[get_property NAME $t],[get_property TILE_TYPE $t]"
	foreach s [get_sites -of_objects $t] {
		puts -nonewline $f ",[get_property NAME $s]:[get_property SITE_TYPE $s]"
	}
	puts $f ""
}
close $f

# Write out IO pin data
set f [open $::env(MEOW_WORK)/raw_io.txt w]
foreach p [get_package_pins] {
	set s [lindex [get_sites -of_objects $p] 0]
	if {$s == ""} { continue } 
	puts -nonewline $f "[get_property NAME $p],[get_property BANK $p],[get_property PIN_FUNC $p],[get_property NAME $s],[get_property SITE_TYPE $s]"
	foreach prop [list_property $p] {
		if {[string range $prop 0 2] != "IS_"} { continue }
		if {[get_property $prop $p] != 1} { continue }
		puts -nonewline $f ",[string range $prop 3 1000]"
	}
	puts $f ""
}

close $f
