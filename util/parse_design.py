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
		self.props = {}
		self.pins = {}
		self.bels = []

	@property
	def cell_type(self):
		return self.props["REF_NAME"]

	@property
	def is_leaf(self):
		return self.props["PRIMITIVE_LEVEL"] == "LEAF"

	@property
	def bel(self):
		assert len(self.bels) == 1
		return self.bels[0]

class Pip:
	def __init__(self, name, node0, node1, is_pseudo):
		self.tile, name = name.split('/')
		name = name.split('.')[1]
		self.wire0, self.wire1 = name.split('->')
		if self.wire1[0] == '>':
			self.wire1 = self.wire1[1:]
		self.node0 = node0
		self.node1 = node1

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
				curr.bels.append(Bel(sl[1]))
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
					curr.pips.append(Pip(sl[1], sl[2], sl[3], int(sl[4])))
			elif sl[0] == '.drv_wire':
				curr.driven_wires.append(DrivenWire(sl[1]))
			elif sl[0] == '.site':
				curr = Site(sl[1])
				des.sites[sl[1]] = curr
				is_site = True

if __name__ == '__main__':
	import sys
	with open(sys.argv[1], 'r') as f:
		Design.parse(f)
