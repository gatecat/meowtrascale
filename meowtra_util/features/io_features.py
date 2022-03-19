from ..parse_design import Design
from ..tilegrid import Tilegrid
from ..device_io import DeviceIO

def io_features(des, extra_cfg):
    f = set()
    g = Tilegrid()

    pin_cfg = {}
    bank_cfg = {}
    if extra_cfg is not None:
        with open(extra_cfg, 'r') as cf:
            for line in cf:
                sl = line.strip().split(' ')
                if len(sl) == 0:
                    continue
                if sl[0] == 'pin':
                    pin_cfg[sl[1]] = dict(x.split('=') for x in sl[3:])
                    pin_cfg[sl[1]]["PIN"] = sl[2]
                if sl[0] == 'bank':
                    bank_cfg[int(sl[1])] = dict(x.split('=') for x in sl[2:])

    for site in des.sites.values():
        if not site.name.startswith("IOB"):
            continue
        t, dx, dy = g.lookup_site(site.name)
        sn = f"{t}.{g.site_type[site.name]}_X{dx}Y{dy}"

        inbuf = None
        outbuf = None
        for cell in site.cells.values():
            for bel in cell.bels:
                if bel.bel == "INBUF": inbuf = cell
                if bel.bel == "OUTBUF": outbuf = cell

        name = inbuf.name if inbuf is not None else (outbuf.name if outbuf is not None else "")
        pin = name.split("_")[0] if name != "" else ""

        if pin not in pin_cfg:
            continue
        cfg = pin_cfg[pin]
        prefix = f"{sn}.{cfg['IOSTANDARD']}"
        f.add(f"{sn}.USED")
        f.add(f"{prefix}.USED")

        if inbuf is not None:
            f.add(f"{sn}.IN")
            f.add(f"{prefix}.IN")
            if outbuf is None:
                f.add(f"{sn}.IN_ONLY")
                f.add(f"{prefix}.IN_ONLY")
            name = inbuf.name
        if outbuf is not None:
            f.add(f"{sn}.OUT")
            f.add(f"{prefix}.OUT")
            if inbuf is not None:
                f.add(f"{sn}.IN_OUT")
                f.add(f"{prefix}.IN_OUT")
            name = outbuf.name

        if "PULLTYPE" in cfg:
            f.add(f"{sn}.PULLTYPE.{cfg['PULLTYPE']}")
        if "SLEW" in cfg:
            if "DRIVE" in cfg:
                f.add(f"{prefix}.DRIVE.{cfg['DRIVE']}.SLEW.{cfg['SLEW']}")
            else:
                f.add(f"{prefix}.SLEW.{cfg['SLEW']}")
        for x in ("DRIVE", "OUTPUT_IMPEDANCE", "EQUALIZATION", "ODT"):
            if x in cfg:
                f.add(f"{prefix}.{x}.{cfg[x]}")
    return f

if __name__ == '__main__':
    import sys
    with open(sys.argv[1], 'r') as f:
        des = Design.parse(f)
    with open(sys.argv[2], 'w') as f:
        f.write('\n'.join(sorted(io_features(des, sys.argv[3] if len(sys.argv) > 3 else None))))
        f.write('\n')
