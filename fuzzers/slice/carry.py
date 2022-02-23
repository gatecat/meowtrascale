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
    for i in range(N_in):
        sig_nets.append(des.add_io(f"i{i}", "IN", pins.next_io()))

    slices = grid.sites_by_type("SLICEL") + grid.sites_by_type("SLICEM")
    r.shuffle(slices)

    def get_sig():
        return sig_nets[r.randrange(max(len(sig_nets) - 64, 0), len(sig_nets))]

    vcc = des.add_net("VCC_0")
    gnd = des.add_net("GND_0")
    des.add(CellInst("vcc0", "VCC", ""))
    des.add(CellInst("gnd0", "GND", ""))
    vcc.add_cell_pin("vcc0", "P")
    gnd.add_cell_pin("gnd0", "G")

    site = ""
    used_sites = set()
    for i in range(N_slice):
        if site != "" and (grid.parse_site(site)[2] % 30) != 29 and r.random() > 0.5:
            parsed_site = grid.parse_site(site)
            site = f"SLICE_X{parsed_site[1]}Y{parsed_site[2]+1}"
        else:
            site = slices.pop()
            while site in used_sites:
                site = slices.pop()
            last_cout = None

        used_sites.add(site)

        o5s = []
        o6s = []
        for j in "ABCDEFGH":
            lut_ins = [get_sig() for i in range(5)]
            o5 = des.add_net(f"o5_{i}{j}")
            o6 = des.add_net(f"o6_{i}{j}")
            for m in (5, 6):
                lut = CellInst(f"lut{i}{j}{m}", "LUT5", f"{site}/{j}{m}LUT")
                lut.params["INIT"] = f"32'h{r.randrange((1<<32)):016x}"
                lut.params["keep"] = 1
                lut.params["dont_touch"] = 1
                des.add(lut)
                for k, inp in enumerate(lut_ins):
                    lut.pins.append((f'I{k}', f'A{5-k}'))
                    inp.add_cell_pin(lut.name, f"I{k}")
                if m == 5:
                    o5.add_cell_pin(lut.name, "O")
                else:
                    o6.add_cell_pin(lut.name, "O")
            o5s.append(o5)
            o6s.append(o6)
            sig_nets.append(o6)

        carry = CellInst(f"carry{i}", "CARRY8", f"{site}/CARRY8")
        o = [des.add_net(f"o{j}_{i}") for j in range(8)]
        co = [des.add_net(f"co{j}_{i}") for j in range(8)]

        single_cy8 = r.random() > 0.3
        carry.params["CARRY_TYPE"] = "SINGLE_CY8" if single_cy8 else "DUAL_CY4"

        if last_cout is not None:
            last_cout.add_cell_pin(carry.name, "CI")
        else:
            r.choice([gnd, vcc, get_sig()]).add_cell_pin(carry.name, "CI")

        if not single_cy8:
            r.choice([gnd, vcc, get_sig()]).add_cell_pin(carry.name, "CI_TOP")

        for j in range(8):
            o6s[j].add_cell_pin(carry.name, f"S[{j}]")
            # choice of DI only if X not used for CI/CI_TOP
            if (j == 0 and last_cout is None) or (j == 4 and not single_cy8):
                o5s[j].add_cell_pin(carry.name, f"DI[{j}]")
            else:
                r.choice([o5s[j], get_sig()]).add_cell_pin(carry.name, f"DI[{j}]")
            o[j].add_cell_pin(carry.name, f"O[{j}]")
            co[j].add_cell_pin(carry.name, f"CO[{j}]")
        for j in range(8):
            sig_nets.append(r.choice([o[j], co[j]]) if j != 7 else o[j])
        last_cout = co[7]
        des.add(carry)

    for i in range(N_out):
        des.add_io(f"o{i}", "OUT", pins.next_io(), sig_nets[-(i+1)])

    des.generate("carry", seed, do_opt=True)

if __name__ == '__main__':
    build(int(sys.argv[1]))
