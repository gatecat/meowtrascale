import os

class CellInst:
    def __init__(self, name, cell_type, bel):
        self.name = name
        self.cell_type = cell_type
        self.bel = bel
        self.params = {}
    def write(self, f):
        print(f"set c [create_cell -reference {self.cell_type} {self.name}]", file=f)
        print(f"place_cell $c {self.bel}", file=f)
        for param, value in sorted(self.params, key=lambda x: x[0]):
            print(f"set_property {param} {value} $c", file=f)
        print(f"set_property IS_BEL_FIXED 1 $c", file=f)
class TopPort:
    def __init__(self, name, direction):
        self.name = name
        self.direction = direction
        self.params = {}
    def write(self, f):
        print(f"set p [create_port -direction {self.direction} {self.name}]", file=f)
        for param, value in sorted(self.params, key=lambda x: x[0]):
            print(f"set_property {param} {value} $p", file=f)
class Net:
    def __init__(self, name):
        self.name = name
        self.pins = set()
    def add_cell_pin(self, name, pin):
        self.pins.add(f"{name}/{pin}")
    def add_top_pin(self, name):
        self.pins.add(f"{name}")
    def write(self, f):
        print(f"set n [create_net {self.name}]", file=f)
        print(f"connect_net -net $n -objects {' '.join(sorted(self.pins))}", file=f)

class Design:
    def __init__(self):
        self.objs = []
    def write(self, f):
        for obj in self.objs:
            obj.write(f)
            print("", file=f)
    def generate(self, name, seed, design, do_route=True):
        with open(f'work/{design}_{seed}.tcl', 'w') as f:
            print(f'create_project -force -part {os.environ["MEOW_PART"]} {design}_{seed} {design}_{seed}', file=f)
            print(f'link_design', file=f)
            self.write(f)
            print(f'place_design', file=f)
            print(f'route_design', file=f)
            print(f'set_property SEVERITY Warning [get_drc_checks]', file=f)
            print(f'set_property BITSTREAM.GENERAL.PERFRAMECRC YES [current_design]')
            print(f'write_checkpoint -force work/{design}_{seed}.dcp', file=f)
            print(f'write_edif -force work/{design}_{seed}.edf', file=f)
            print(f'write_bitstream -force work/{design}_{seed}.bit', file=f)
            print(f'source $::env(MEOW_UTIL)/tcl/dump_design.tcl', file=f)
            print(f'dump_design work/{design}_{seed}.dump', file=f)
