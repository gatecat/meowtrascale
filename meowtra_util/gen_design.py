import os
from pathlib import Path

class CellInst:
    def __init__(self, name, cell_type, bel):
        self.name = name
        self.cell_type = cell_type
        self.bel = bel
        self.params = {}
        self.pins = []
    def write(self, f):
        print(f"set c [create_cell -reference {self.cell_type} {self.name}]", file=f)
        for param, value in sorted(self.params.items(), key=lambda x: x[0]):
            print(f"set_property {param} {value} $c", file=f)
        if self.bel != "":
            print(f"place_cell $c {self.bel}", file=f)
            if "/" in str(self.bel):
                print(f"set_property IS_BEL_FIXED 1 $c", file=f)
            print(f"set_property IS_LOC_FIXED 1 $c", file=f)
        if len(self.pins) > 0:
            print(f"set_property LOCK_PINS {{{' '.join(f'{k}:{v}' for k, v in self.pins)}}} $c", file=f)
class TopPort:
    def __init__(self, name, direction):
        self.name = name
        self.direction = direction
        self.params = {}

    def write(self, f):
        print(f"set p [create_port -direction {self.direction} {self.name}]", file=f)
        for param, value in sorted(self.params.items(), key=lambda x: x[0]):
            print(f"set_property {param} {value} $p", file=f)
class Net:
    def __init__(self, name):
        self.name = name
        self.pins = set()
        self.params = {}

    def add_cell_pin(self, name, pin):
        self.pins.add(f"{name}/{pin}")
    def add_top_pin(self, name):
        self.pins.add(f"{name}")
    def write(self, f):
        print(f"set n [create_net {self.name}]", file=f)
        print(f"connect_net -net $n -objects {{{' '.join(sorted(self.pins))}}}", file=f)
        for param, value in sorted(self.params.items(), key=lambda x: x[0]):
            print(f"set_property {param} {value} $n", file=f)
class Design:
    def __init__(self):
        self.objs = []
        self.site_pip_fuzz = []

    def add(self, obj):
        self.objs.append(obj)
    def write(self, f):
        # write nets last
        for obj in self.objs:
            if not isinstance(obj, Net):
                obj.write(f)
                print("", file=f)
        for obj in self.objs:
            if isinstance(obj, Net):
                obj.write(f)
                print("", file=f)
    def add_io(self, name, direction, site, int_net=None):
        assert direction in ("IN", "OUT")
        top_port = TopPort(name, direction)
        top_net = Net(name)
        top_iob = CellInst(name + "_buf", "IBUF" if direction == "IN" else "OBUF", site)
        top_net.add_top_pin(name)
        top_net.add_cell_pin(top_iob.name, "I" if direction == "IN" else "O")
        if int_net is None:
            int_net = Net(name + ("_i" if direction == "IN" else "_o"))
            self.add(int_net)
        int_net.add_cell_pin(top_iob.name, "O" if direction == "IN" else "I")
        self.add(top_port)
        self.add(top_net)
        self.add(top_iob)
        return int_net

    def add_net(self, name):
        net = Net(name)
        self.add(net)
        return net

    def add_site_pip_fuzzer(self, site):
        self.site_pip_fuzz.append(site)

    def generate(self, design, seed, do_route=True, do_opt=False):
        Path("work").mkdir(parents=True, exist_ok=True)
        with open(f'work/{design}_{seed}.tcl', 'w') as f:
            print(f'create_project -force -part {os.environ["MEOW_PART"]} {design}_{seed} {design}_{seed}', file=f)
            print(f'link_design', file=f)
            self.write(f)
            print(f'write_checkpoint -force {design}_{seed}_preroute.dcp', file=f)
            if do_opt:
                print(f'opt_design', file=f)
            print(f'place_design', file=f)
            print(f'route_design', file=f)
            if len(self.site_pip_fuzz) > 0:
                print(f'source $::env(MEOW_UTIL)/tcl/fuzz_site_pips.tcl', file=f)
                for site in self.site_pip_fuzz:
                    print(f'fuzz_site_pips [get_sites {site}]', file=f)
            print(f'set_property SEVERITY Warning [get_drc_checks]', file=f)
            print(f'set_property BITSTREAM.GENERAL.PERFRAMECRC YES [current_design]', file=f)
            print(f'write_checkpoint -force {design}_{seed}.dcp', file=f)
            print(f'write_edif -force {design}_{seed}.edf', file=f)
            print(f'write_bitstream -force {design}_{seed}.bit', file=f)
            print(f'source $::env(MEOW_UTIL)/tcl/dump_design.tcl', file=f)
            print(f'dump_design {design}_{seed}.dump', file=f)

def generate_verilog_tcl(design, verilog, seed, top="top"):
    Path("work").mkdir(parents=True, exist_ok=True)
    with open(f'work/{design}_{seed}.v', 'w') as f:
        f.write(verilog)
    with open(f'work/{design}_{seed}.tcl', 'w') as f:
        print(f'create_project -force -part {os.environ["MEOW_PART"]} {design}_{seed} {design}_{seed}', file=f)
        print(f'read_verilog {os.getcwd()}/work/{design}_{seed}.v', file=f)
        print(f'link_design -top {top}', file=f)
        print(f'write_checkpoint -force {design}_{seed}_preroute.dcp', file=f)
        print(f'place_design', file=f)
        print(f'route_design', file=f)
        print(f'set_property SEVERITY Warning [get_drc_checks]', file=f)
        print(f'set_property BITSTREAM.GENERAL.PERFRAMECRC YES [current_design]', file=f)
        print(f'write_checkpoint -force {design}_{seed}.dcp', file=f)
        print(f'write_edif -force {design}_{seed}.edf', file=f)
        print(f'write_bitstream -force {design}_{seed}.bit', file=f)
        print(f'source $::env(MEOW_UTIL)/tcl/dump_design.tcl', file=f)
        print(f'dump_design {design}_{seed}.dump', file=f)
