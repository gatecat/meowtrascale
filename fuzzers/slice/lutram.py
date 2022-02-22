import sys
from random import Random
from meowtra_util.gen_design import *
from meowtra_util.tilegrid import Tilegrid
from meowtra_util.device_io import DeviceIO

N_in = 16
N_out = 16
N_glb = 16
N_slice = 300

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

    slices = grid.sites_by_type("SLICEM")
    r.shuffle(slices)

    def get_sig():
        return sig_nets[r.randrange(max(len(sig_nets) - 64, 0), len(sig_nets))]

    for i in range(N_slice):
        site = slices.pop()

        def get_ctlsig():
            return r.choice(glb_nets) if r.random() > 0.25 else get_sig()

        clk = get_ctlsig()
        clkinv = r.choice(["1'b1", "1'b0"])
        we = get_ctlsig()
        ram32 = r.random() > 0.75
        wa_width = 5 if ram32 else r.randint(6, 9)
        num_luts = 2 if ram32 and r.random() > 0.5 else 1
        wa = [get_sig() for k in range(wa_width)]

        for m in range(num_luts):
            output = des.add_net(f"n{i}Hlut{m}")
            output.params["CLOCK_DEDICATED_ROUTE"] = "FALSE"
            lut = CellInst(f"lut{i}H{m}", "RAMS32" if ram32 else "RAMS64E1", f"{site}/H5LUT" if m == 1 else f"{site}/H6LUT")
            lut.params["IS_CLK_INVERTED"] = clkinv
            lut.params["keep"] = 1
            lut.params["dont_touch"] = 1
            clk.add_cell_pin(lut.name, "CLK")
            we.add_cell_pin(lut.name, "WE")
            get_sig().add_cell_pin(lut.name, "I")
            output.add_cell_pin(lut.name, "O")
            for k, inp in enumerate(wa):
                inp.add_cell_pin(lut.name, f"WADR{k}" if k >= 6 else f"ADR{k}")
                # lut.pins.append((f'ADR{k}', f'A{k + 1}'))
            sig_nets.append(output)
            des.add(lut)

        for j in "ABCDEFG":
            if r.random() > 0.5:
                continue
            ra = [get_sig() for k in range(5 if ram32 else 6)]
            for m in range(num_luts):
                output = des.add_net(f"n{i}{j}lut{m}")
                output.params["CLOCK_DEDICATED_ROUTE"] = "FALSE"
                if ram32:
                    prim = "RAMD32"
                elif wa_width == 8:
                    prim = "RAMS64E1"
                else:
                    prim = "RAMD64E"
                lut = CellInst(f"lut{i}{j}{m}", prim, f"{site}/{j}5LUT" if m == 1 else f"{site}/{j}6LUT")
                lut.params["IS_CLK_INVERTED"] = clkinv
                lut.params["keep"] = 1
                lut.params["dont_touch"] = 1
                clk.add_cell_pin(lut.name, "CLK")
                we.add_cell_pin(lut.name, "WE")
                get_sig().add_cell_pin(lut.name, "I")
                output.add_cell_pin(lut.name, "O")
                if prim == "RAMS64E1":
                    for k, inp in enumerate(wa):
                        inp.add_cell_pin(lut.name, f"WADR{k}" if k >= 6 else f"ADR{k}")
                        # if k < 6:
                        #    lut.pins.append((f'ADR{k}', f'A{k + 1}'))
                else:
                    for k, inp in enumerate(wa):
                        inp.add_cell_pin(lut.name, f"WADR{k}")
                    for k, inp in enumerate(ra):
                        inp.add_cell_pin(lut.name, f"RADR{k}")
                        # lut.pins.append((f'RADR{k}', f'A{k + 1}'))
                sig_nets.append(output)
                des.add(lut)

    for i in range(N_out):
        des.add_io(f"o{i}", "OUT", pins.next_io(), sig_nets[-(i+1)])

    des.generate("lutram", seed, do_opt=True)

if __name__ == '__main__':
    build(int(sys.argv[1]))
