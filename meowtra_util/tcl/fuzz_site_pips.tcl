proc fuzz_site_pips {site} {
	set_property MANUAL_ROUTING TRUE $site
	set sitepips [get_property SITE_PIPS $site]
	foreach sp [get_site_pips -of_objects $site] {
		if { rand() > 0.1 } {
			continue
		}
		if {[string match "*INV*" $sp]} {
			continue
		}
		if {[string match "*LUT*" $sp]} {
			continue
		}
		regex {([A-Z0-9_]+)/([A-Z0-9_]+):([A-Z0-9_]+)} $sp match sp_site sp_bel sp_pin
		if {[string first $sp_bel $sitepips] != -1} {
			continue
		}
		puts $sp
		append sitepips " " $sp_bel ":" $sp_pin
	}
	puts $sitepips
	set_property SITE_PIPS $sitepips $site
}
