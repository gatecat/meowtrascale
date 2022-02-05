import os

def Tilegrid:
    def __init__(self):
        self.tiles = {}
        self.tile_sites = {}
        with open(os.environ['MEOW_WORK'] + "/raw_tiles.txt", "r") as f:
            for line in f:
                sl = line.strip().split(',')
                if len(sl) == 0:
                    continue
                col = int(sl[0])
                row = int(sl[1])
                clkr = sl[2]
                name = sl[3]
                tile_type = sl[4]
                sites = [tuple(x.split(":")) for x in sl[5:]]
                self.tiles[(col, row)] = (name, tile_type, clkr)
                self.tile_sites[name] = sites
