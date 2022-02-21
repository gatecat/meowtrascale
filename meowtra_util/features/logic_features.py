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
        latch = [False, False]
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
                    if "6LUT" in b and any(c.bel is not None and c.bel.bel == b.replace('6', '5') for c in site.cells.values()):
                        has_56lut = True
                    if "5LUT" in b and any(c.bel is not None and c.bel.bel == b.replace('5', '6') for c in site.cells.values()):
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
                                log = cell.phys_pins.get(f'A{j+1}', [])
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
            elif "FF" in b:
                if cell.cell_type[0:2] == "FD" and cell.cell_type[3] == "E":
                    is_latch = False
                    clkpin = "C"
                    cepin = "CE"
                    if cell.cell_type[2] == "P":
                        is_sync = False
                        srval = 1
                        srpin = "PRE"
                    elif cell.cell_type[2] == "C":
                        is_sync = False
                        srval = 0
                        srpin = "CLR"
                    elif cell.cell_type[2] == "S":
                        is_sync = True
                        srval = 1
                        srpin = "S"
                    elif cell.cell_type[2] == "R":
                        is_sync = True
                        srval = 0
                        srpin = "R"
                elif cell.cell_type[0:2] == "LD" and cell.cell_type[3] == "E":
                    is_latch = True
                    clkpin = "G"
                    cepin = "GE"
                    if cell.cell_type[2] == "P":
                        is_sync = False
                        srval = 1
                        srpin = "PRE"
                    elif cell.cell_type[2] == "C":
                        is_sync = False
                        srval = 0
                        srpin = "CLR"
                else:
                    continue
                prim = "LATCH" if is_latch else "FF"
                if "INIT" in cell.props:
                    f.add(f"{t}.{b}.{prim}.INIT.{cell.props['INIT'].parse()}")
                f.add(f"{t}.{b}.{prim}.SRVAL.{srval}")
                half = 1 if b[0] in "EFGH" else 0
                two = 1 if b[-1] == "2" else 0
                latch[half] = is_latch
                ffsync[half] = is_sync
                clkinv[half] = f"IS_{clkpin}_INVERTED" in cell.props and cell.props[f"IS_{clkpin}_INVERTED"].parse() == 1
                srinv[half] = f"IS_{srpin}_INVERTED" in cell.props and cell.props[f"IS_{srpin}_INVERTED"].parse() == 1
                haveff[half] = True
                srused[half * 2 + two] = srpin in cell.pins and len(cell.pins[srpin]) > 0
                ceused[half * 2 + two] = cepin in cell.pins and len(cell.pins[cepin]) > 0
            elif "CARRY8" in b:
                if cell.cell_type != "CARRY8":
                    continue
                if "CARRY_TYPE" in cell.props:
                    f.add(f"{t}.{b}.CARRY_TYPE.{cell.props['CARRY_TYPE'].value}")
                carry_pins = ["CI", "DI[0]", "DI[1]", "DI[2]", "DI[3]", "DI[4]", "DI[5]", "DI[6]", "DI[7]", "CI_TOP"]
                for p in carry_pins:
                    if p not in cell.conns or cell.conns[p] is None:
                        continue # no logical net
                    if p in cell.pins and len(cell.pins[p]) > 0:
                        phys = cell.pins[p][0]
                        pn = p.replace('[', '').replace(']', '')
                        f.add(f"{t}.{b}.{pn}.{phys}")
                    elif p in ('CI', 'CI_TOP'):
                        n = cell.conns[p]
                        if n == 'GLOBAL_LOGIC0':
                            f.add(f"{t}.{b}.{p}.0")
                        elif n == 'GLOBAL_LOGIC1':
                            f.add(f"{t}.{b}.{p}.1")

        for i in range(2):
            # site-wide ff config
            if not haveff[i]:
                continue
            half = "EFGHFF" if i == 1 else "ABCDFF"
            prim = "LATCH" if latch[i] else "FF"
            f.add(f"{t}.{half}.{prim}")
            f.add(f"{t}.{half}.{prim}.CLKINV.{int(clkinv[i])}")
            if ffsync[i]: f.add(f"{t}.{half}.{prim}.SYNC")
            for j in range(2):
                if srused[i * 2 + j]:
                    f.add(f"{t}.{half}{'2' if j == 1 else ''}.SRUSED")
                    f.add(f"{t}.{half}.{prim}.SRINV.{int(srinv[i])}")
                if ceused[i * 2 + j]: f.add(f"{t}.{half}{'2' if j == 1 else ''}.CEUSED")
        for pip in site.pips:
            if "INV" in pip.bel: continue
            f.add(f"{t}.{pip.bel}.{pip.inp}")
    return f

if __name__ == '__main__':
    import sys
    with open(sys.argv[1], 'r') as f:
        des = Design.parse(f)
    with open(sys.argv[2], 'w') as f:
        f.write('\n'.join(sorted(logic_features(des))))
        f.write('\n')
