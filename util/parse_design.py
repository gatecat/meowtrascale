class Property:
    def __init__(self, value):
        self.value = value
    def parse(self):
        if "'h" in self.value:
            return int(self.value.split("'h")[1], 16)
        elif "'b" in self.value:
            return int(self.value.split("'h")[1], 2)
        else:
            return int(self.value)

class Bel:
    def __init__(self, name):
        (self.site, self.bel) = name.split('/')

class Cell:
    def __init__(self, name):
        self.name = name
        self.props = {}
        self.pins = {}
        self._phys_pins = None
        self.bels = []

    @property
    def cell_type(self):
        return self.props["REF_NAME"].value

    @property
    def phys_pins(self):
        if self._phys_pins is None:
            self._phys_pins = {}
            for l, phys in self.pins.items():
                for p in phys:
                    if p not in self._phys_pins:
                        self._phys_pins[p] = []
                    self._phys_pins[p].append(l)
        return self._phys_pins

    @property
    def is_leaf(self):
        return self.props["PRIMITIVE_LEVEL"].value == "LEAF"

    @property
    def bel(self):
        if len(self.bels) != 1:
            return None
        return self.bels[0]

class Pip:
    def __init__(self, name, node0, node1, flags):
        self.tile, name = name.split('/')
        name = name.split('.')[1]
        self.wire0, self.wire1 = name.split('->')
        if self.wire1[0] == '>':
            self.wire1 = self.wire1[1:]
        self.node0 = node0
        self.node1 = node1
        self.is_pseudo = "P" in flags
        self.is_bidi = "B" in flags
        self.is_inverted = "N" in flags
        self.is_fixed_inv = "I" in flags

class DrivenWire:
    def __init__(self, name):
        self.tile, self.wire = name.split('/')

class Net:
    def __init__(self, name):
        self.name = name
        self.route = None
        self.pips = []
        self.driven_wires = []

class SitePip:
    def __init__(self, name):
        self.bel, self.inp = name.split('/')[1].split(':')

class Site:
    def __init__(self, name):
        self.name = name
        self.pips = []
        self.cells = {}

class Design:
    def __init__(self):
        self.cells = {}
        self.nets = {}
        self.sites = {}
    def parse(f):
        des = Design()
        curr = None
        is_site = False
        for line in f:
            sl = line.strip().split(' ')
            if sl[0] == '.cell':
                curr = Cell(sl[1])
                des.cells[sl[1]] = curr
            elif sl[0] == '.prop':
                curr.props[sl[1]] = Property(" ".join(sl[2:]))
            elif sl[0] == '.pin':
                curr.pins[sl[1].rpartition('/')[-1]] = [x.rpartition('/')[-1] for x in sl[2:]]
            elif sl[0] == '.bel':
                bel = Bel(sl[1])
                curr.bels.append(bel)
                if bel.site not in des.sites:
                    des.sites[bel.site] = Site(bel.site)
                des.sites[bel.site].cells[curr.name] = curr
            elif sl[0] == '.net':
                curr = Net(sl[1])
                des.nets[sl[1]] = curr
                is_site = False
            elif sl[0] == '.route':
                curr.route = ' '.join(sl[2:]) # TODO: actually parse
            elif sl[0] == '.pip':
                if is_site:
                    curr.pips.append(SitePip(sl[1]))
                else:
                    curr.pips.append(Pip(sl[1], sl[2], sl[3], sl[4] if len(sl) >= 5 else ""))
            elif sl[0] == '.drv_wire':
                curr.driven_wires.append(DrivenWire(sl[1]))
            elif sl[0] == '.site':
                if sl[1] not in des.sites:
                    des.sites[sl[1]] = Site(sl[1])
                curr = des.sites[sl[1]]
                is_site = True
        return des

if __name__ == '__main__':
    import sys
    with open(sys.argv[1], 'r') as f:
        Design.parse(f)
