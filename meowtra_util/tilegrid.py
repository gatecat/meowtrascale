import os

class Tilegrid:
    def __init__(self):
        self.tiles = {}
        self.tile_sites = {}
        self.site_in_tile = None
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
    def parse_site(self, site):
        prefix, _, xy = site.rpartition('_')
        assert xy[0] == 'X'
        x, y = xy[1:].split('Y')
        return prefix, int(x), int(y)
    def lookup_site(self, site):
        if self.site_in_tile is None:
            self.site_in_tile = {}
            for tile, sites in self.tile_sites.items():
                split_sites = [self.parse_site(x[0]) for x in sites]
                # compute relative site coordinates in tile
                min_xy = {}
                for prefix, x, y in split_sites:
                    if prefix not in min_xy:
                        min_xy[prefix] = (x, y)
                    else:
                        min_xy[prefix] = (min(min_xy[prefix][0], x), min(min_xy[prefix][1], y))
                for tile_site, (prefix, x, y) in zip(sites, split_sites):
                    self.site_in_tile[tile_site[0]] = (tile, (x - min_xy[prefix][0]), (y - min_xy[prefix][1]))
        return self.site_in_tile[site]
    def sites_by_type(self, site_type):
        result = []
        for _, sites in sorted(self.tile_sites.items(), key=lambda x: x[0]):
            result += [s for s, st in sites if st == site_in_tile]
        return result
