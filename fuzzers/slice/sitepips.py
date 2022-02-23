import sys
from random import Random
from meowtra_util.gen_design import *
from meowtra_util.tilegrid import Tilegrid
from meowtra_util.device_io import DeviceIO

N_slice = 300

def build(seed):
    r = Random(seed)
    des = Design()
    grid = Tilegrid()
    slices = grid.sites_by_type("SLICEL") + grid.sites_by_type("SLICEM")
    r.shuffle(slices)
    des.add(CellInst("vcc0", "VCC", ""))
    vcc = des.add_net("VCC_0")
    vcc.add_cell_pin("vcc0", "P")
    for i in range(N_slice):
        site = slices.pop()
        for j in "ABCDEFGH":
            lut = CellInst(f"lut{i}{j}", "LUT1", f"{site}/{j}6LUT")
            lut.params["INIT"] = "2'h0"
            lut.params["keep"] = 1
            lut.params["dont_touch"] = 1
            vcc.add_cell_pin(lut.name, "I0")
            des.add(lut)
        des.add_site_pip_fuzzer(site)
    des.generate("sitepips", seed, do_opt=True)

if __name__ == '__main__':
    build(int(sys.argv[1]))
