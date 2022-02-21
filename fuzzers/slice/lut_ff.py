import sys
from random import Random
from meowtra_util.gen_design import *
from meowtra_util.tilegrid import Tilegrid
from meowtra_util.device_io import DeviceIO

N_in = 16
N_out = 16
N_glb = 16
N_slice = 2000

def build(seed):
    r = Random(seed)
    des = Design()
    grid = Tilegrid()
    pins = DeviceIO()
    sig_nets = []
    glb_nets = []
    for i in range(N_in):
        sig_nets.append(des.add_io(f"i{i}", "IN", pins.next_io()))

    for i in range(N_glb):
        glb_nets.append(des.add_io(f"ck{i}", "IN", pins.next_clk()))

    slices = grid.sites_by_type("SLICEL") + grid.sites_by_type("SLICEM")
    r.shuffle(slices)

    def get_sig():
        return sig_nets[r.randrange(max(len(sig_nets) - 64, 0), len(sig_nets))]

    def get_srpin(fftype):
        if "CE" in fftype: return "CLR"
        if "PE" in fftype: return "PRE"
        if "SE" in fftype: return "S"
        if "RE" in fftype: return "R"
        assert False, fftype

    for i in range(N_slice):
        site = slices.pop()
        for j in "ABCDEFGH":
            # setup control sets
            if j == "A" or j == "E":
                def get_ffsig():
                    return r.choice(glb_nets) if r.random() > 0.25 else get_sig()
                clk = get_ffsig()
                sr = r.choice([get_ffsig(), None])
                ce = [r.choice([get_ffsig(), None]) for k in range(2)]
                clkinv = r.choice(["1'b1", "1'b0"])
                srinv = r.choice(["1'b1", "1'b0"])
                fftypes = r.choice(["FDRE_FDSE", "FDCE_FDPE", "LDCE_LDPE"])

            if r.random() > 0.75:
                # pick 6 random inputs from at most the last 64 signals
                inputs = [get_sig() for k in range(6)]
                output = des.add_net(f"n{i}{j}lut")
                output.params["CLOCK_DEDICATED_ROUTE"] = "FALSE"
                sig_nets.append(output)
                lut = CellInst(f"lut{i}{j}", "LUT6", f"{site}/{j}6LUT")
                lut.params["INIT"] = f"64'h{r.randrange((1<<64)):016x}"
                for k, inp in enumerate(inputs):
                    lut.pins.append((f'I{k}', f'A{6-k}'))
                    inp.add_cell_pin(lut.name, f"I{k}")
                output.add_cell_pin(lut.name, "O")
                des.add(lut)

            if r.random() > 0.25:
                # make FF
                bel = r.choice(["2", ""])
                fftype = r.choice(fftypes.split("_"))
                ff = CellInst(f"ff{i}{j}", fftype, f"{site}/{j}FF{bel}")
                output = des.add_net(f"n{i}{j}ff")
                sig_nets.append(output)
                output.params["CLOCK_DEDICATED_ROUTE"] = "FALSE"
                clkpin = "G" if "LD" in fftype else "C"
                clk.add_cell_pin(ff.name, clkpin)
                ff.params[f"IS_{clkpin}_INVERTED"] = clkinv
                ff.params[f"INIT"] = r.choice(["1'b1", "1'b0"])
                if sr is not None:
                    srpin = get_srpin(fftype)
                    sr.add_cell_pin(ff.name, srpin)
                    ff.params[f"IS_{srpin}_INVERTED"] = srinv
                if ce[1 if bel == "2" else 0] is not None:
                    cepin = "GE" if "LD" in fftype else "CE"
                    ce[1 if bel == "2" else 0].add_cell_pin(ff.name, cepin)
                in_sig = sig_nets[-2] if r.random() > 0.5 else get_sig()
                in_sig.add_cell_pin(ff.name, "D")
                output.add_cell_pin(ff.name, "Q")
                des.add(ff)

    for i in range(N_out):
        des.add_io(f"o{i}", "OUT", pins.next_io(), sig_nets[-(i+1)])

    des.generate("lut_ff", seed)

if __name__ == '__main__':
    build(int(sys.argv[1]))
