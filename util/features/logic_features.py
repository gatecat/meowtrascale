from ..parse_design import Design
from ..tilegrid import Tilegrid

def logic_features(des):
    f = set()
    g = Tilegrid()
    for site in des.sites.values():
        if not site.name.startswith("SLICE_"):
            continue
        t, dx, dy = g.lookup_site(site.name)
        assert dx == 0 and dy == 0
        clkinv = [False, False]
        srinv  = [False, False]
        ffsync = [False, False]
        haveff = [False, False]
        srused = [False, False, False, False]
        ceused = [False, False, False, False]
        for cell in site.cells.values():
            if not cell.is_leaf:
                continue
            b = cell.bel.bel
            if "LUT" in b:
                prefix = f"{t}.{b.replace('5', '').replace('6', '')}"
                if cell.cell_type.startswith("RAM"):
                    f.add(f"{prefix}.RAM")
                    if "32" in cell.cell_type:
                        f.add(f"{prefix}.RAM.SMALL")
                    else:
                        f.add(f"{prefix}.RAM.LARGE")
                if "SRL" in cell.cell_type:
                    f.add(f"{prefix}.SRL")
                if "SRL16" in cell.cell_type:
                    f.add(f"{prefix}.SRL.SMALL")
                if "SRLC32" in cell.cell_type:
                    f.add(f"{prefix}.SRL.LARGE")
                for i in range(6, 9):
                    if f"WADR{i}" in cell.phys_pins:
                        f.add(f"{prefix}.WA{i+1}USED")
                if "SRLC32" in cell.cell_type:
                    if "D" in cell.pins and len(cell.pins["D"]) == 1:
                        f.add(f"{prefix}.DI.{cell.pins['D'][0]}")
                elif "DI1" in cell.phys_pins:
                    f.add(f"{prefix}.DI1.DI1")
                if "RAM" in cell.cell_type or "SRL" in cell.cell_type and "IS_CLK_INVERTED" in cell.props:
                    f.add(f"LCLKINV.{cell.props['IS_CLK_INVERTED'].parse()}")
                if "INIT" in cell.props:
                    # skip writing INIT features if both LUTs present
                    has_56lut = False
                    if "6LUT" in b and any(c.bel == b.replace('6', '5') for c in site.cells.values()):
                        has_56lut = True
                    if "5LUT" in b and any(c.bel == b.replace('5', '6') for c in site.cells.values()):
                        has_56lut = True
                    if not has_56lut and cell.cell_type.startswith("LUT"):
                        init = cell.props["INIT"].parse()
                        for i in range(32 if "5LUT" in b else 64):
                            init_idx = 0
                            valid_idx = True
                            for j in range(6):
                                # look at log to phys mapping to depermute init
                                if (i & (1 << j)) == 0:
                                    continue
                                log = cell.phys_pins.get(f'A{i+1}', [])
                                if len(log) == 0:
                                    continue
                                log = log[0]
                                if log.startswith("RADR"):
                                    init_idx |= (1 << int(log[-1]))
                                elif log.startswith("A["):
                                    init_idx |= (1 << int(log[2]))
                                elif not log.startswith("I") and not log.startswith("A"):
                                    valid_idx = False
                                    break
                                else:
                                    init_idx |= (1 << int(log[-1]))
                            if not valid_idx:
                                continue
                            if (init & (1 << init_idx)) == 0:
                                continue
                            f.add(f"{prefix}.INIT[{i}]")
    return f

if __name__ == '__main__':
    import sys
    with open(sys.argv[1], 'r') as f:
        des = Design.parse(f)
    with open(sys.argv[2], 'w') as f:
        f.write('\n'.join(sorted(logic_features(des))))
        f.write('\n')
