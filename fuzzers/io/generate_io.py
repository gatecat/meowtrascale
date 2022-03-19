import sys
from random import Random
from meowtra_util.gen_design import *
from meowtra_util.tilegrid import Tilegrid
from meowtra_util.device_io import DeviceIO

def build(seed):
    r = Random(seed)
    des = Design()
    grid = Tilegrid()
    pins = DeviceIO()

    bank_to_pins = {}
    bank_to_iotype = {}
    site_to_pin = {}

    for pin in pins.pins.values():
        if not pin.general:
            continue
        if pin.bank not in bank_to_pins:
            bank_to_pins[pin.bank] = []
        bank_to_pins[pin.bank].append(pin)
        if pin.bank not in bank_to_iotype:
            bank_to_iotype[pin.bank] = pin.site_type.split("_")[0]
        site_to_pin[pin.site] = pin

    used_pins = []
    io_config = []

    bank_to_vcc = {}
    bank_pod_used = set()
    bank_pod_not_used = set()
    lut_inputs = []
    lut_outputs = []
    bank_iref = {}
    bound_pins = set()

    def inp():
        sig = des.add_net(f"li[{len(lut_inputs)}]")
        lut_inputs.append(sig)
        return sig
    def outp():
        sig = des.add_net(f"lo[{len(lut_outputs)}]")
        lut_outputs.append(sig)
        return sig

    for b, t in sorted(bank_to_iotype.items(), key=lambda x: x[0]):
        if t == "HPIOB":
            bank_to_vcc[b] = r.choice(
                ["1.0", "1.2", "1.35", "1.5", "1.8"]
            )
        elif t == "HDIOB":
            bank_to_vcc[b] = r.choice(
                ["1.2", "1.5", "1.8", "2.5", "3.3"]
            )
        else:
            assert False, t

    standards = {
        ("HPIOB", "1.8"): ["LVCMOS18", "LVDCI_18", "HSLVDCI_18",
                    "HSTL_I_18", "HSTL_I_DCI_18",
                    "SSTL18_I", "SSTL18_I_DCI"],
        ("HPIOB", "1.5"): ["LVCMOS15", "LVDCI_15", "HSLVDCI_15",
                    "HSTL_I", "HSTL_I_DCI",
                    "SSTL15", "SSTL15_DCI"],
        ("HPIOB", "1.35"): ["SSTL135", "SSTL135_DCI"],
        ("HPIOB", "1.2"): ["LVCMOS12", "HSTL_I_12", "HSTL_I_DCI_12",
                    "SSTL12", "SSTL12_DCI",
                    "HSUL_12", "HSUL_12_DCI",
                    "POD12", "POD12_DCI"],
        ("HPIOB", "1.0"): ["POD10", "POD10_DCI"],

        ("HDIOB", "3.3"): ["LVCMOS33", "LVTTL"],
        ("HDIOB", "2.5"): ["LVCMOS25"],
        ("HDIOB", "1.8"): ["LVCMOS18"],
        ("HDIOB", "1.5"): ["LVCMOS15"],
        ("HDIOB", "1.2"): ["LVCMOS12"]
    }

    prims = {
        "HPIOB": ["IBUF", "OBUF", "OBUFT", "IOBUF", "IOBUFE3", "IBUF_IBUFDISABLE"],
        "HDIOB": ["IBUF","OBUF", "OBUFT", "IOBUF"]
    }

    drives = {
        ("HPIOB", "LVCMOS18"): ["2", "4", "6", "8", "12"],
        ("HPIOB", "LVCMOS15"): ["2", "4", "6", "8", "12"],
        ("HPIOB", "LVCMOS12"): ["2", "4", "6", "8"],
        ("HDIOB", "LVCMOS33"): ["4", "8", "12", "16"],
        ("HDIOB", "LVCMOS25"): ["4", "8", "12", "16"],
        ("HDIOB", "LVCMOS18"): ["4", "8", "12", "16"],
        ("HDIOB", "LVCMOS15"): ["4", "8", "12", "16"],
        ("HDIOB", "LVCMOS12"): ["4", "8", "12"],
    }

    for pin in pins.pins.values():
        if not pin.general:
            continue
        if "VREF" in pin.pin_func:
            continue # conflict
        if "VRP" in pin.pin_func or "VRN" in pin.pin_func:
            continue # conflict
        if r.randint(1, 3) == 1:
            continue # improved fuzzing
        params = {}
        iot = pin.site_type.split("_")[0]
        assert (iot, bank_to_vcc[pin.bank]) in standards, pin
        ios = r.choice(standards[iot, bank_to_vcc[pin.bank]])
        if pin.bank in bank_pod_used and ios in ("HSTL_I_12", "HSTL_I_DCI_12", "SSTL12", "SSTL12_DCI", "HSUL_12", "HSUL_12_DCI"):
            ios = "LVCMOS12"
        if pin.bank in bank_pod_not_used and ios in ("POD12", "POD12_DCI"):
            ios = "LVCMOS12"
        params["IOSTANDARD"] = ios
        prim = r.choice(prims[iot])
        params["prim"] = prim
        if prim in ("IBUF", "IOBUF", "IOBUFE3", "IBUF_IBUFDISABLE", "IBUF_INTERMDISABLE"):
            params["PULLTYPE"] = r.choice(["NONE", "PULLUP", "PULLDOWN", "KEEPER"])
        if prim in ("OBUF", "OBUFT", "IOBUF", "IOBUFE3"):
            if (iot, ios) in drives:
                params["DRIVE"] = r.choice(drives[iot, ios])
            if iot == "HPIOB":
                params["SLEW"] = r.choice(["SLOW", "MEDIUM", "FAST"])
            else:
                params["SLEW"] = r.choice(["SLOW", "FAST"])
            if iot == "HPIOB" and ("POD" in ios or "SSTL" in ios):
                params["OUTPUT_IMPEDANCE"] = r.choice(["RDRV_40_40", "RDRV_48_48", "RDRV_60_60"])
        if prim in ("IBUF", "IOBUF", "IOBUFE3", "IBUF_IBUFDISABLE", "IBUF_INTERMDISABLE"):
            if "POD" in ios:
                odt_choices = []
                if "DCI" not in ios:
                    odt_choices.append("RTT_NONE")
                if "OUTPUT_IMPEDANCE" not in params or params["OUTPUT_IMPEDANCE"] != "RDRV_48_48":
                    odt_choices += ["RTT_40", "RTT_60"]
                else:
                    odt_choices += ["RTT_48"]
                params["ODT"] = r.choice(odt_choices)
                if "POD12" in ios:
                    params["EQUALIZATION"] = r.choice(["EQ_LEVEL0", "EQ_LEVEL1", "EQ_LEVEL2", "EQ_LEVEL3", "EQ_LEVEL4", "EQ_NONE"])
        used_pins.append(pin)
        io_config.append(params)
        if iot == "HDIOB":
            if ios in ( "HSTL_I_18", "SSTL18_I", "SSTL18_II"):
                bank_iref[pin.bank] = "0.90"
            elif ios in ( "HSTL_I", "SSTL15", "SSTL15_II"):
                bank_iref[pin.bank] = "0.75"
            elif ios in ("SSTL135", "SSTL135_II"):
                bank_iref[pin.bank] = "0.675"
            elif ios == "SSTL12":
                bank_iref[pin.bank] = "0.60"
        if ios in ("POD12", "POD12_DCI"):
            bank_pod_used.add(pin.bank)
            bank_iref[pin.bank] = r.choice([None, "0.84"])
        if ios in ("HSTL_I_12", "HSTL_I_DCI_12", "SSTL12", "SSTL12_DCI", "HSUL_12", "HSUL_12_DCI"):
            bank_pod_not_used.add(pin.bank)
            bank_iref[pin.bank] = r.choice([None, "0.60"])

    for i, params in enumerate(io_config):
        prim = params["prim"]
        pin = used_pins[i]
        if prim in ("IBUF", "IBUF_IBUFDISABLE"):
            direction = "IN"
            iob_p = "I"
        elif prim in ("OBUF", "OBUFT"):
            direction = "OUT"
            iob_p = "O"
        elif prim in ("IOBUF", "IOBUFE3"):
            direction = "INOUT"
            iob_p = "IO"
        top_port = TopPort(f"p{i}", direction)
        for k, v in sorted(params.items()):
            if k.isupper():
                top_port.params[k] = v
        des.add(top_port)
        port_net = des.add_net(f"p{i}")
        port_net.add_top_pin(f"p{i}")

        iob = CellInst(f"p{i}_iob", prim, f"{pin.site}")
        port_net.add_cell_pin(iob.name, iob_p)
        if prim in ("IBUF", "IBUF_IBUFDISABLE", "IOBUF", "IOBUFE3") and r.random() > 0.5:
            inp().add_cell_pin(iob.name, "O")
        if prim in ("OBUF", "OBUFT", "IOBUF", "IOBUFE3") and r.random() > 0.5:
            outp().add_cell_pin(iob.name, "I")
        if prim in ("OBUFT", "IOBUF", "IOBUFE3") and r.random() > 0.5:
            outp().add_cell_pin(iob.name, "T")
        if prim in ("IBUF_IBUFDISABLE", "IOBUFE3") and r.random() > 0.5:
            outp().add_cell_pin(iob.name, "IBUFDISABLE")
        if prim == "IOBUFE3" and r.random() > 0.5:
            outp().add_cell_pin(iob.name, "DCITERMDISABLE")
        iob.params['keep'] = True
        iob.params['dont_touch'] = True
        des.add(iob)
    for bank, iref in sorted(bank_iref.items()):
        if iref is None:
            continue
        des.extra_config.append(f"set_property INTERNAL_VREF {iref} [get_iobanks {bank}]")
    slices = grid.sites_by_type("SLICEL")
    for i in range(max(len(lut_inputs)//6 + 1, len(lut_outputs))):
        sl = slices.pop()
        ip = [lut_inputs[(i * 6 + j) % len(lut_inputs)] for j in range(6)]
        lut = CellInst(f"l{i}", "LUT6", f"{sl}/A6LUT")
        lut.params['keep'] = True
        lut.params['dont_touch'] = True
        for j in range(6):
            ip[j].add_cell_pin(lut.name, f"I{j}")
        if i < len(lut_outputs):
            lut_outputs[i].add_cell_pin(lut.name, "O")
        des.add(lut)
    des.generate("io", seed, do_opt=True)
    with open(f"work/io_{seed}_config.txt", "w") as f: # params that dump_design misses
        for i, params in enumerate(io_config):
            pin = used_pins[i]
            cfg = " ".join(f'{k}={v}' for k, v in params.items())
            print(f'pin p{i} {pin} {cfg}', file=f)
        for bank, iref in sorted(bank_iref.items()):
            if iref is None:
                continue
            print(f'bank {iref} INTERNAL_VREF={iref}', file=f)
if __name__ == '__main__':
    build(int(sys.argv[1]))
