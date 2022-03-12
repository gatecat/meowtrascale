proc dump_design {out_name} {
	# Parameters too boring to dump...
	set skip_params {CLASS HIERARCHICALNAME IS_BOUNDARY_INST IS_DEBUGGABLE IS_FIXED IS_LOC_FIXED PRIMITIVE_COUNT PRIMITIVE_GROUP PRIMITIVE_SUBGROUP PRIMITIVE_TYPE}

	set fp [open $out_name w]
	foreach cell [get_cells] {
		puts $fp ".cell [get_property NAME $cell]"
		foreach prop [list_property $cell] {
			if { [lsearch -sorted $skip_params $prop] != -1 } {
				continue
			}
			set value [get_property $prop $cell]
			if { $value != "" } {
				puts $fp ".prop $prop [get_property $prop $cell]"
			}
		}
		foreach bel [get_bels -of_objects $cell] {
			puts $fp ".bel $bel"
		}
		foreach pin [get_pins -of_objects $cell] {
			puts $fp ".pin $pin [get_bel_pins -of_objects $pin]"
			puts $fp ".conn $pin [get_nets -of_objects $pin]"
		}
	}

	set seen_vcc 0
	set seen_gnd 0

	foreach net [get_nets] {
		puts $fp ".net [get_property NAME $net]"
		foreach pin [get_site_pins -of_objects $net] {
			puts $fp ".sitepin $pin [get_property DIRECTION $pin] [get_nodes -of_objects $pin]"
		}
		if {[get_property TYPE $net] == "GROUND"} {
			if {$seen_gnd == 1} { continue }
			set seen_gnd 1
		}
		if {[get_property TYPE $net] == "POWER"} {
			if {$seen_vcc == 1} { continue }
			set seen_vcc 1
		}
		puts $fp ".route [get_property ROUTE $net]"
		foreach pip [get_pips -of_objects $net] {
			set n0 [get_nodes -uphill -of_objects $pip ]
			set n1 [get_nodes -downhill -of_objects $pip ]
			puts -nonewline $fp ".pip $pip $n0 $n1 "
			if {[get_property IS_INVERTED $pip] == 1} { puts -nonewline $fp "N" }
			if {[get_property IS_FIXED_INVERSION $pip] == 1} { puts -nonewline $fp "I" }
			if {[get_property IS_DIRECTIONAL $pip] == 0} { puts -nonewline $fp "B" }
			if {[get_property IS_PSEUDO $pip] == 1} { puts -nonewline $fp "P" }
			puts $fp ""
			foreach wire [get_wires -of_objects $n1] {
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
	close $fp
}
