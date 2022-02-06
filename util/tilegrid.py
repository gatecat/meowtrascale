import os

class Tilegrid:
    def __init__(self):
        self.tiles = {}
        self.tile_sites = {}
        self.width = 0
        self.height = 0
        self.clock_width = 0
        self.clock_height = 0
        with open(os.environ['MEOW_WORK'] + "/raw_tiles.txt", "r") as f:
            for line in f:
                sl = line.strip().split(',')
                if len(sl) == 0:
                    continue
                col = int(sl[0])
                row = int(sl[1])
                self.width = max(self.width, col+1)
                self.height = max(self.height, row+1)
                clkr = self.parse_clock(sl[2]) if sl[2] != '' else None
                if clkr is not None:
                    self.clock_width = max(self.clock_width, clkr[0] + 1)
                    self.clock_height = max(self.clock_height, clkr[1] + 1)
                name = sl[3]
                tile_type = sl[4]
                sites = [tuple(x.split(":")) for x in sl[5:]]
                self.tiles[(col, row)] = (name, tile_type, clkr)
                self.tile_sites[name] = sites
    def parse_clock(self, cr):
        assert cr[0] == 'X'
        x, y = cr[1:].split('Y')
        return int(x), int(y)