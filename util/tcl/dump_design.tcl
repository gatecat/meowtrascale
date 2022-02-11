set fp [open $::env(MEOW_OUT) w]
foreach cell [get_cells] {
	puts $fp ".cell [get_property NAME $cell]"
	foreach prop [list_property $cell] {
		set value [get_property $prop $cell]
		if { $value != "" } {
			puts $fp ".prop $prop [get_property $prop $cell]"
		}
	}
	foreach bel [get_bels -of_objects $cell] {
		puts $fp ".bel $pin [get_bel_pins -of_objects $pin]"
	}
	foreach pin [get_pins -of_objects $cell] {
		puts $fp ".pin $pin [get_bel_pins -of_objects $pin]"
	}
}

foreach net [get_nets] {
	puts $fp ".net [get_property NAME $net]"
	puts $fp ".route [get_property ROUTE $net]"
	foreach pip [get_pips -of_objects $net] {
		puts $fp ".pip $pip"
		foreach wire [get_wires -of_objects [get_nodes -downhill -of_objects $pip ]] {
			puts $fp ".drv_wire $wire"
		}
	}
}

foreach site [get_sites -filter { IS_USED == 1 } ] {
	puts $fp ".site [get_property NAME $site]"
	foreach pip [get_site_pips -filter { IS_USED == 1 } -of_objects $site] {
		puts $fp ".pip $pip"
	}
}
