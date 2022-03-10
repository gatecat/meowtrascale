# The purpose of this script is to dump enough of a routing graph to be able to fuzz otherwise-difficult RCLK/CMT bits

create_project -force -part $::env(MEOW_PART) empty_design empty_design
link_design

# RCLKs themselves
set f [open $::env(MEOW_WORK)/clk_graph.txt w]

# for dumping near-to-rclk routes, too
proc do_visit {node depth} {
	global f
	global visited

	if { $depth > 5 } {
		return
	}
	if { [info exists visited($node)] } {
		return
	}
	set visited($node) $depth
	set pip_count 0
	foreach pip [get_pips -uphill -of_objects $node] {
		# only interested in leaving the RCLK...
		set pip_t [get_tiles -of_objects $pip]
		set pip_tt [get_property TILE_TYPE $pip_t]

		if {[string first "RCLK" $pip_tt] == 0 || [string first "CMT" $pip_tt] == 0 || [string first "CLE" $pip_tt] == 0} {
			continue
		}

		incr pip_count
		if {$pip_tt == "INT" && ($pip_count > 1) } {
			break
		}

		puts $f "extpip $pip_t [get_nodes -uphill -of_objects $pip] [get_nodes -downhill -of_objects $pip]"
		do_visit [get_nodes -uphill -of_objects $pip] [expr $depth+1]
	}
	set pip_count 0
	foreach pip [get_pips -downhill -of_objects $node] {
		# only interested in leaving the RCLK...
		set pip_t [get_tiles -of_objects $pip]
		set pip_tt [get_property TILE_TYPE $pip_t]

		if {[string first "RCLK" $pip_tt] == 0 || [string first "CMT" $pip_tt] == 0 || [string first "CLE" $pip_tt] == 0} {
			continue
		}

		incr pip_count
		if {$pip_tt == "INT" && ($pip_count > 1) } {
			break
		}

		puts $f "extpip $pip_t [get_nodes -uphill -of_objects $pip] [get_nodes -downhill -of_objects $pip]"
		do_visit [get_nodes -downhill -of_objects $pip] [expr $depth+1]
	}
	foreach site_pin [get_site_pins -of_objects $node] {
		puts $f "extpin $site_pin $node"
	}
}

foreach t [get_tiles] {
	set tt [get_property TILE_TYPE $t]
	if {[string first "RCLK" $tt] != 0 && [string first "CMT" $tt] != 0} {
		continue
	}
	foreach pip [get_pips -of_objects $t] {
		puts $f "pip [get_property NAME $pip] [get_property IS_DIRECTIONAL $pip] [get_nodes -uphill -of_objects $pip] [get_nodes -downhill -of_objects $pip]"
		do_visit [get_nodes -uphill -of_objects $pip] 0
		do_visit [get_nodes -downhill -of_objects $pip] 0
	}
	foreach site [get_sites -of_objects $t] {
		foreach site_pin [get_site_pins -of_objects $site] {
			puts $f "pin $site_pin [get_nodes -of_objects $site_pin]"
		}
	}
}
close $f
