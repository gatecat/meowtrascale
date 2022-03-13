from ..parse_design import Design
from ..tilegrid import Tilegrid

def process_net(f, net, const_nets):
    # solve bidirectional pips
    unsolved = []
    driven_nodes = set()
    backward_pips = set()
    for pin in net.pins:
        if pin.direction == "OUT":
            driven_nodes.add(pin.node)
    for pip in net.pips:
        if not pip.is_bidi:
            driven_nodes.add(pip.node1)
        else:
            unsolved.append(pip)
    while len(unsolved) > 0:
        unsolved_next = []
        for pip in unsolved:
            if pip.node0 in driven_nodes:
                driven_nodes.add(pip.node1)
            elif pip.node1 in driven_nodes:
                driven_nodes.add(pip.node0)
                backward_pips.add((pip.node0, pip.node1))
            else:
                unsolved_next.append(pip)
        unsolved = unsolved_next
    # write out pips
    for pip in net.pips:
        if pip.tile.startswith("BRAM_"):
            continue
        if pip.tile.startswith("INT_INTF_"):
            if "LOGIC_OUTS" in pip.wire1:
                f.add(f"{pip.tile}.WIRE.LOGIC_OUTS_R.DRIVEN")
                continue
        postfix = ""
        if pip.is_bidi:
            if (pip.node0, pip.node1) in backward_pips:
                postfix += ".BWD"
            else:
                postfix += ".FWD"
        if pip.is_inverted and not pip.is_fixed_inv:
            postfix += ".INV"
        f.add(f"{pip.tile}.PIP.{pip.wire1}.{pip.wire0}{postfix}")
    # write out extra features
    if net.name not in const_nets:
        for wire in net.driven_wires:
            if wire.tile.startswith("RCLK"):
                if any(wire.wire.startswith(x) for x in \
                    ("CLK_TEST_BUF_SITE_", "CLK_HROUTE_CORE_OPT", "CLK_VDISTR_BOT", 
                        "CLK_VDISTR_TOP", "CLK_VROUTE_BOT", "CLK_VROUTE_TOP")):
                    f.add(f"{wire.tile}.WIRE.{wire.wire}.USED")
                if "CLKBUF" in wire.tile or wire.tile.startswith("RCLK_BRAM_INTF_L") or \
                    wire.tile.startswith("RCLK_RCLK_XIPHY_INNER_FT") or \
                    wire.tile.startswith("RCLK_HDIO") or wire.tile.startswith("GTH_QUAD_RIGHT"):
                    if wire.wire.startswith("CLK_HROUTE") or wire.wire.startswith("CLK_HDISTR"):
                        f.add(f"{wire.tile}.WIRE.{wire.wire}.USED")
            if wire.tile.startswith("CMT_"):
                if any(wire.wire.startswith(x) for x in \
                    ("CLK_TEST_BUF_SITE_", "CLK_HROUTE_", "CLK_HDISTR_", "CLK_VDISTR_")):
                    f.add(f"{wire.tile}.WIRE.{wire.wire}.USED")

def route_features(des):
    f = set()
    const_nets = set()
    for cell in des.cells.values():
        if cell.cell_type == "GND" and "G" in cell.conns:
            const_nets.add(cell.conns["G"])
        if cell.cell_type == "VCC" and "P" in cell.conns:
            const_nets.add(cell.conns["P"])
    for net in des.nets.values():
        process_net(f, net, const_nets=const_nets)
    # write out default-configured cell pseudo-features to decorrelate site and routing bits
    g = Tilegrid()
    for cell in des.cells.values():
        if cell.name[0] != 'c' or not cell.name[1:].isdigit():
            continue
        if not cell.is_leaf or cell.bel is None:
            continue
        t, dx, dy = g.lookup_site(cell.bel.site)
        f.add(f"{t}.{cell.bel.site.rsplit('_', 2)[0]}_X{dx}Y{dy}.{cell.bel.bel}.__DEFAULT")
    return f

if __name__ == '__main__':
    import sys
    with open(sys.argv[1], 'r') as f:
        des = Design.parse(f)
    with open(sys.argv[2], 'w') as f:
        f.write('\n'.join(sorted(route_features(des))))
        f.write('\n')
