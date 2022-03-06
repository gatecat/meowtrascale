#include "tools.h"
#include "context.h"
#include "cmdline.h"
#include "log.h"
#include "database.h"
#include "constids.h"
#include "nodegraph.h"
#include "rng.h"
#include "datafile.h"

#include <fstream>
#include <limits>
#include <queue>

MEOW_NAMESPACE_BEGIN

struct FuzzGenWorker {
    FuzzGenWorker(CmdlineResult args) : args(args), ctx(),
        graph(NodeGraph::parse(&ctx, args.named.at("nodegraph").at(0))) {};
    CmdlineResult args;
    Context ctx;
    NodeGraph graph;
    DeterministicRNG rng;

    struct TileTypePip {
        TileTypePip() = default;
        TileTypePip(IdString tile_type, IdString pip_name) : tile_type(tile_type), pip_name(pip_name) {};
        IdString tile_type;
        IdString pip_name;
        bool operator==(const TileTypePip &other) const {
            return tile_type == other.tile_type && pip_name == other.pip_name;
        }
        unsigned hash() const {
            return mkhash(tile_type.hash(), pip_name.hash());
        }
    };

    struct TileTypeData {
        IdString name;
        index_t pip_count = 0;
        idict<TileKey> tile_insts;
        std::vector<dict<IdString, index_t>> tile_pips;
    };

    std::vector<TileTypeData> tile_types;
    idict<IdString> tile_type_keys;

    pool<TileTypePip> avail_ttpips;
    dict<TileTypePip, int> covered_ttpips;
    std::vector<TileTypePip> try_pips;

    void setup_tiletypes() {
        for (index_t i = 0; i < index_t(graph.pips.size()); i++) {
            auto &pip = graph.pips.at(i);
            if (!pip.is_rclk)
                continue;
            IdString tile_type = pip.tile.prefix;
            index_t type_idx = tile_type_keys(tile_type);
            if (type_idx == index_t(tile_types.size()))
                tile_types.emplace_back();
            auto &type_data = tile_types.at(type_idx);
            type_data.name = tile_type;
            index_t inst_idx = type_data.tile_insts(pip.tile);
            if (inst_idx == index_t(type_data.tile_pips.size()))
                type_data.tile_pips.emplace_back();
            type_data.tile_pips.at(inst_idx)[pip.pip_name] = i;
            avail_ttpips.emplace(tile_type, pip.pip_name);
        }
        for (auto &tt : tile_types) {
            for (auto &inst : tt.tile_pips)
                tt.pip_count = std::max(tt.pip_count, index_t(inst.size()));
        }
        for (auto ttp : avail_ttpips)
            covered_ttpips[ttp] = 0;
    }

    dict<index_t, index_t> node2net;
    dict<index_t, std::vector<index_t>> net2route;
    pool<index_t> used_pips;


    std::vector<std::string> string_pool;
    index_t string_pool_idx = 0;
    const char *node_name(index_t node) {
        if (string_pool.empty())
            string_pool.resize(100);
        if (string_pool_idx >= 100)
            string_pool_idx = 0;
        auto &entry = string_pool.at(string_pool_idx);
        entry.clear();
        entry += graph.nodes.at(node).tile.str(&ctx);
        entry += '/';
        entry += graph.nodes.at(node).name.str(&ctx);
        return string_pool.at(string_pool_idx++).c_str();
    }

    const int iter_limit = 500000;

    bool do_route(int net_idx, TileTypePip ttpip) {
        auto &tt = tile_types.at(tile_type_keys(ttpip.tile_type));
        auto &ti = tt.tile_pips.at(rng.rng(tt.tile_pips.size()));
        if (!ti.count(ttpip.pip_name))
            return false;
        index_t pip = ti.at(ttpip.pip_name);
        auto &pip_data = graph.pips.at(pip);
        if (node2net.count(pip_data.src_node) || node2net.count(pip_data.dst_node))
            return false;
        index_t src_node = pip_data.src_node;
        index_t dst_node = pip_data.dst_node;
        if (pip_data.bidi && rng.rng(2))
            std::swap(src_node, dst_node);
        dict<index_t, index_t> visited_fwd, visited_bwd;
        if (verbose_flag)
            log_verbose("trying %s --> %s\n", node_name(src_node), node_name(dst_node));
        std::queue<index_t> queue_fwd, queue_bwd;
        visited_fwd[dst_node] = pip;
        visited_fwd[src_node] = -1; // disallow in fwd cone
        queue_fwd.push(dst_node);

        int fwd_iter = 0;
        index_t fwd_endpoint = -1;
        while (!queue_fwd.empty() && fwd_iter++ < iter_limit) {
            index_t cursor = queue_fwd.front();
            queue_fwd.pop();
            auto &cursor_node = graph.nodes.at(cursor);
            if (!cursor_node.pins.empty()) {
                for (auto &pin : cursor_node.pins) {
                    if (!process_sitepin(net_idx, pin, true))
                        continue;
                    fwd_endpoint = cursor;
                    break;
                }
                if (fwd_endpoint != -1)
                    break;
            }
            for (index_t pip_dh : cursor_node.pips_downhill) {
                if (used_pips.count(pip_dh))
                    continue;
                auto &cursor_pip = graph.pips.at(pip_dh);
                if (node2net.count(cursor_pip.dst_node) || visited_fwd.count(cursor_pip.dst_node))
                    continue;
                visited_fwd[cursor_pip.dst_node] = pip_dh;
                queue_fwd.push(cursor_pip.dst_node);
            }
        }

        if (fwd_endpoint == -1)
            return false; // no endpoint reached

        visited_bwd[src_node] = pip;
        // disallow fwd cone in bwd cone
        index_t fwd_cursor = fwd_endpoint;
        while (fwd_cursor != dst_node) {
            visited_bwd[fwd_cursor] = -1;
            auto &fwd_pip = graph.pips.at(visited_fwd.at(fwd_cursor));
            fwd_cursor = fwd_pip.src_node;
        }
        visited_bwd[dst_node] = -1; // disallow in bwd cone
        queue_bwd.push(src_node);

        int bwd_iter = 0;
        index_t bwd_startpoint = -1;
        while (!queue_bwd.empty() && bwd_iter++ < iter_limit) {
            index_t cursor = queue_bwd.front();
            queue_bwd.pop();
            auto &cursor_node = graph.nodes.at(cursor);
            if (!cursor_node.pins.empty()) {
                for (auto &pin : cursor_node.pins) {
                    if (!process_sitepin(net_idx, pin, false))
                        continue;
                    bwd_startpoint = cursor;
                    break;
                }
                if (bwd_startpoint != -1)
                    break;
            }
            for (index_t pip_uh : cursor_node.pips_uphill) {
                if (used_pips.count(pip_uh))
                    continue;
                auto &cursor_pip = graph.pips.at(pip_uh);
                if (node2net.count(cursor_pip.src_node) || visited_bwd.count(cursor_pip.src_node))
                    continue;
                visited_bwd[cursor_pip.src_node] = pip_uh;
                queue_bwd.push(cursor_pip.src_node);
            }
        }

        if (bwd_startpoint == -1)
            return false; // no startpoint found

        log_verbose("    success (%d fwd %d bwd)!\n", fwd_iter, bwd_iter);

        // do bind
        used_pips.insert(pip);
        node2net[src_node] = net_idx;
        node2net[dst_node] = net_idx;
        std::vector<index_t> fwd_nodes, bwd_nodes;
        fwd_cursor = fwd_endpoint;
        log_verbose("    fwd path:\n");
        while (fwd_cursor != dst_node) {
            fwd_nodes.push_back(fwd_cursor);
            if (verbose_flag)
                log_verbose("        %s\n", node_name(fwd_cursor));
            node2net[fwd_cursor] = net_idx;
            auto &fwd_pip = graph.pips.at(visited_fwd.at(fwd_cursor));
            fwd_cursor = fwd_pip.src_node;
        }
        fwd_nodes.push_back(dst_node);
        log_verbose("    bwd path:\n");
        index_t bwd_cursor = bwd_startpoint;
        while (bwd_cursor != src_node) {
            bwd_nodes.push_back(bwd_cursor);
            if (verbose_flag)
                log_verbose("        %s\n", node_name(bwd_cursor));
            node2net[bwd_cursor] = net_idx;
            auto &bwd_pip = graph.pips.at(visited_bwd.at(bwd_cursor));
            bwd_cursor = bwd_pip.dst_node;
        }
        bwd_nodes.push_back(src_node);
        std::reverse(fwd_nodes.begin(), fwd_nodes.end());
        net2route[net_idx].clear();
        for (auto node : bwd_nodes)
            net2route[net_idx].push_back(node);
        for (auto node : fwd_nodes)
            net2route[net_idx].push_back(node);
        covered_ttpips[ttpip] += 1;
        return true;
    }

    struct CellInst {
        int cell_idx = -1;
        IdString cell_type;
        dict<IdString, int> pin2net;
    };
    dict<std::pair<TileKey, IdString>, CellInst> cells;

    bool add_net_pin(int net, TileKey site, IdString bel, IdString cell_type, IdString cell_pin) {
#if 0
        auto site_str = site.str(&ctx);
        log_verbose("adding pin %s/%s/%s on net %d\n", site_str.c_str(), bel.c_str(&ctx), cell_pin.c_str(&ctx), net);
#endif
        auto &cell_inst = cells[std::make_pair(site, bel)];
        if (cell_inst.cell_type == IdString()) {
            cell_inst.cell_type = cell_type;
            cell_inst.cell_idx = int(cells.size());
        } else {
            MEOW_ASSERT(cell_inst.cell_type == cell_type);
        }
        if (cell_inst.pin2net.count(cell_pin))
            return false;
        cell_inst.pin2net[cell_pin] = net;
        return true;
    }

    bool process_sitepin(int net, SitePin pin, bool is_input) {
        TileKey site = pin.site;
        if (site.prefix.in(id_BUFGCE, id_BUFGCE_DIV, id_BUFG_PS, id_BUFG_GT)) {
            IdString bel = (site.prefix == id_BUFGCE) ? id_BUFCE : site.prefix;
            if (pin.pin == id_CE_PRE_OPTINV && is_input)
                return add_net_pin(net, site, bel, site.prefix, id_CE);
            if (pin.pin == id_CLK_IN && is_input)
                return add_net_pin(net, site, bel, site.prefix, id_I);
            if (pin.pin == id_CLK_OUT && !is_input)
                return add_net_pin(net, site, bel, site.prefix, id_O);
        } else if (site.prefix == id_BUFGCE_HDIO) {
            if (pin.pin == id_CLK_IN && is_input)
                return add_net_pin(net, site, id_BUFCE, id_BUFGCE, id_I);
            if (pin.pin == id_CLK_OUT && !is_input)
                return add_net_pin(net, site, id_BUFCE, id_BUFGCE, id_O);
        } else if ((site.prefix == id_BUFCE_ROW || site.prefix == id_BUFCE_ROW_FSR) && pin.pin == id_CLK_OUT && !is_input) {
            return add_net_pin(net, site, id_BUFCE, id_BUFCE_ROW, id_O);
        } else if (site.prefix == id_BUFCE_LEAF) {
            if (pin.pin == id_CLK_IN && is_input)
                return add_net_pin(net, site, id_BUFCE, id_BUFCE_LEAF, id_I);
            else if (pin.pin == id_CE_INT && is_input)
                return add_net_pin(net, site, id_BUFCE, id_BUFCE_LEAF, id_CE);
            return false; // return log churn
        } else if (site.prefix == id_SLICE) {
            if ((pin.pin == id_CLK1 || pin.pin == id_CLK2) && is_input) {
                return add_net_pin(net, site, (pin.pin == id_CLK2) ? id_EFF : id_AFF, id_FDRE, id_C);
            } else if ((pin.pin == id_SRST1 || pin.pin == id_SRST2) && is_input) {
                return add_net_pin(net, site, (pin.pin == id_SRST2) ? id_EFF : id_AFF, id_FDRE, id_R);
            } else if ((pin.pin == id_CKEN1 || pin.pin == id_CKEN2) && is_input) {
                return add_net_pin(net, site, (pin.pin == id_CKEN2) ? id_EFF : id_AFF, id_FDRE, id_CE);
            } else if (!is_input) {
                const std::string &pin_str = pin.pin.str(&ctx);
                if (pin_str.size() == 3 && pin_str.substr(1) == "_O")
                    return add_net_pin(net, site, ctx.id(stringf("%c6LUT", pin_str.at(0))), id_LUT1, id_O);
                if (pin_str.size() >= 2 && pin_str.at(1) == 'Q')
                    return add_net_pin(net, site, ctx.id(stringf("%cFF%s", pin_str.at(0), pin_str.c_str() + 2)), id_FDRE, id_Q);
            }
        } else if (site.prefix == id_MMCM) {
            if (is_input ? pin.pin.in(id_CLKIN1, id_CLKIN2, id_CLKFBIN) :
                    pin.pin.in(id_CLKFBOUT, id_CLKFBOUTB, id_CLKOUT0, id_CLKOUT0B, id_CLKOUT1, id_CLKOUT1B, id_CLKOUT2, id_CLKOUT2B,
                        id_CLKOUT3, id_CLKOUT3B, id_CLKOUT4, id_CLKOUT5, id_CLKOUT6))
                return add_net_pin(net, site, id_MMCM, id_MMCME4_ADV, pin.pin);
                } else if (site.prefix == id_MMCM) {
        } else if (site.prefix == id_PLL) {
            if (is_input ? pin.pin.in(id_CLKIN, id_CLKFBIN) :
                    pin.pin.in(id_CLKFBOUT, id_CLKOUT0, id_CLKOUT0B, id_CLKOUT1, id_CLKOUT1B, id_CLKOUTPHY))
                return add_net_pin(net, site, id_PLL, id_PLLE4_ADV, pin.pin);
        } else if (site.prefix == id_BITSLICE_RX_TX) {
            if (is_input && pin.pin.in(id_RX_CLK, id_TX_CLK))
                return add_net_pin(net, site, id_RXTX_BITSLICE, id_RXTX_BITSLICE, pin.pin);
            return false; // return log churn
        } else if (site.prefix == id_IOB && pin.pin == id_I && !is_input) {
            return add_net_pin(net, site, IdString(), id_IBUF, id_O);
        }
#if 1
        if (!site.prefix.in(id_GCLK_TEST_BUFE3, id_BUFCE_LEAF, id_BUFCE_ROW, id_BUFCE_ROW_FSR) && verbose_flag) {
            auto site_str = site.str(&ctx);
            log_verbose("skipping %s pin %s/%s\n", (is_input ? "input" : "output"),  site_str.c_str(), pin.pin.c_str(&ctx));
        }
#endif
        return false;
    }

    void write_tcl(int design) {
        std::string filename = args.named.at("out").at(0);
        filename += "/gen_design_";
        filename += std::to_string(design);
        filename += ".tcl";
        std::ofstream out(filename);
        if (!out)
            log_error("failed to open output file %s\n", filename.c_str());
        out << "remove_net *" << std::endl;
        out << "if {[llength [get_cells]] > 0} { set_property dont_touch 0 [get_cells] }" << std::endl; // for remove_cell to work (useful for interactive tests)
        out << "remove_cell *" << std::endl;
        out << std::endl;
        dict<int, std::vector<std::pair<int, IdString>>> net2pin;
        for (auto &cell : cells) {
            int idx = cell.second.cell_idx;
            out << "set c [create_cell -reference " << cell.second.cell_type.c_str(&ctx) << " c" << idx << "]" << std::endl;
            if (cell.first.second == IdString())
                out << "place_cell $c " << cell.first.first.str(&ctx) << std::endl;
            else
                out << "place_cell $c " << cell.first.first.str(&ctx) << "/" << cell.first.second.c_str(&ctx) << std::endl;
            out << "set_property keep 1 $c" << std::endl;
            out << "set_property dont_touch 1 $c" << std::endl;
            out << "set_property IS_LOC_FIXED 1 $c" << std::endl;
            if (cell.first.second != IdString())
                out << "set_property IS_BEL_FIXED 1 $c" << std::endl;
            out << std::endl;
            for (auto pin : cell.second.pin2net) {
                net2pin[pin.second].emplace_back(cell.second.cell_idx, pin.first);
            }
            if (cell.second.cell_type == id_IBUF) {
                out << "set p [create_port -direction IN c" << idx << "_io]" << std::endl;
                out << "set n [create_net c" << idx << "_io]" << std::endl;
                out << "connect_net -net $n -objects {c" << idx << "_io c" << idx << "/I}" << std::endl;
            }
        }
        for (auto &net : net2route) {
            int idx = net.first;
            out << "set n [create_net n" << idx << "]" << std::endl;
            std::string pins = "";
            for (auto pin : net2pin.at(idx)) {
                if (!pins.empty())
                    pins += " ";
                pins += stringf("c%d/%s", pin.first, pin.second.c_str(&ctx));
            }
            out << "connect_net -net $n -objects {" << pins << "}" << std::endl;
            std::string route = "";
            for (index_t node : net.second) {
                if (!route.empty())
                    route += " ";
                route += node_name(node);
            }
            out << "set_property ROUTE {" << route << "} $n" << std::endl;
            out << "set_property IS_ROUTE_FIXED 1 $n" << std::endl;
            out << std::endl;
        }
    }

    void setup_design() {
        node2net.clear();
        used_pips.clear();
        cells.clear();
        net2route.clear();
        try_pips.clear();
        // sort pips randomly first, then by lowest coverage
        for (auto ttp : avail_ttpips)
            try_pips.push_back(ttp);
        rng.shuffle(try_pips);
        std::stable_sort(try_pips.begin(), try_pips.end(), [&] (TileTypePip a, TileTypePip b) {
            return covered_ttpips.at(a) < covered_ttpips.at(b);
        });
        // disable a random percentage of nodes
        for (index_t i = 0; i < index_t(graph.nodes.size()); i++) {
            if (rng.rng(10) < 2)
                node2net[i] = -1;
        }
    }

    void print_coverage() {
        dict<IdString, int> tt_coverage;
        for (auto cov : covered_ttpips) {
            if (cov.second > 0)
                tt_coverage[cov.first.tile_type] += 1;
        }
        for (auto &tt : tile_types) {
            log_info("%30s %6d %6d\n", tt.name.c_str(&ctx), tt_coverage.count(tt.name) ? tt_coverage.at(tt.name) : 0, tt.pip_count);
        }
    }

    int operator()() {
        if (args.named.count("seed"))
            rng.rngseed(parse_u32(args.named.at("seed").at(0)));
        else
            rng.rngseed(1);
        setup_tiletypes();
        log_info("Total of %d tiletype-pips to test\n", int(avail_ttpips.size()));
        int net = 0;
        for (int design = 0; design < (args.named.count("num-designs") ? std::stoi(args.named.at("num-designs").at(0)) : 20); design++) {
            setup_design();
            for (int i = 0; i < 50000; i++) {
                net++;
                do_route(net, try_pips.at(i % int(try_pips.size())));
            }
            write_tcl(design);
        }
        print_coverage();
        return 0;
    }
};

int subcmd_fuzztools(int argc, const char *argv[]) {
    CmdlineParser parser;
    parser.add_opt("v", 0, "verbose output");
    parser.add_opt("nodegraph", 1, "path to nodegraph file");
    parser.add_opt("num-designs", 1, "number of designs to generate");
    parser.add_opt("out", 1, "output directory");

    CmdlineResult result;
    if (!parser.parse(argc, argv, 2, std::cerr, result))
        return 1;

    if (result.named.count("v"))
        verbose_flag = true;

    FuzzGenWorker worker(result);
    return worker();

    Context ctx;
}

MEOW_NAMESPACE_END
