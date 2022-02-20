import sys
from random import Random
from meowtra_util.gen_design import *
from meowtra_util.tilegrid import Tilegrid
from meowtra_util.device_io import DeviceIO

N_in = 16
N_out = 16
N_slice = 2000

def build(seed):
    r = Random(seed)
    des = Design()
    grid = Tilegrid()
    pins = DeviceIO()
    sig_nets = []
    for i in range(N_in):
        sig_nets.append(des.add_io(f"i{i}", "IN", pins.next_io()))

    slices = grid.sites_by_type("SLICEL") + grid.sites_by_type("SLICEM")
    r.shuffle(slices)

    for i in range(N_slice):
        site = slices.pop()
        for j in "ABCDEFGH":
            if r.random() > 0.25:
                continue # quarter density
            # pick 6 random inputs from at most the last 64 signals
            inputs = [sig_nets[r.randrange(max(len(sig_nets) - 64, 0), len(sig_nets))] for k in range(6)]
            output = des.add_net(f"n{i}{j}")
            sig_nets.append(output)
            lut = CellInst(f"lut{i}{j}", "LUT6", f"{site}/{j}6LUT")
            lut.params["INIT"] = f"64'h{r.randrange((1<<64)):016x}"
            for k, inp in enumerate(inputs):
                lut.pins.append((f'I{k}', f'A{6-k}'))
                inp.add_cell_pin(lut.name, f"I{k}")
            output.add_cell_pin(lut.name, "O")
            des.add(lut)

    for i in range(N_out):
        des.add_io(f"o{i}", "OUT", pins.next_io(), sig_nets[-(i+1)])

    des.generate("lut_init", seed)

if __name__ == '__main__':
    build(int(sys.argv[1]))
