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
    pool<TileKey> used_sites, routethru_sites;


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

    bool is_routethru_blocked(const Pip &pip_data) {
        // don't route through used sites
        auto &from_node = graph.nodes.at(pip_data.src_node);
        auto &to_node = graph.nodes.at(pip_data.dst_node);
        for (auto fpin : from_node.pins) {
            if (!used_sites.count(fpin.site))
                continue;
            for (auto tpin : to_node.pins) {
                if (!used_sites.count(tpin.site))
                    continue;
                return true;
            }
        }
        return false;
    }

    void block_sites(const Pip &pip_data) {
        auto &from_node = graph.nodes.at(pip_data.src_node);
        auto &to_node = graph.nodes.at(pip_data.dst_node);
        for (auto fpin : from_node.pins) {
            for (auto tpin : to_node.pins) {
                if (fpin.site == tpin.site)
                    routethru_sites.insert(fpin.site);
            }
        }
    }

    bool do_route(int net_idx, TileTypePip ttpip, bool is_rare) {
        auto &tt = tile_types.at(tile_type_keys(ttpip.tile_type));
        // Up to 2-20 attempts
        for (int i = 0; i < (is_rare ? 20 : 2); i++) {
            auto &ti = tt.tile_pips.at(rng.rng(tt.tile_pips.size()));
            if (!ti.count(ttpip.pip_name))
                continue;
            index_t pip = ti.at(ttpip.pip_name);
            auto &pip_data = graph.pips.at(pip);
            if (node2net.count(pip_data.src_node) || node2net.count(pip_data.dst_node) || is_routethru_blocked(pip_data))
                continue;
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
            bool vcc_allowed = false;
            SitePin dst_pin{TileKey(), IdString()};
            while (!queue_fwd.empty() && fwd_iter++ < iter_limit) {
                index_t cursor = queue_fwd.front();
                queue_fwd.pop();
                auto &cursor_node = graph.nodes.at(cursor);
                if (!cursor_node.pins.empty() && (rng.rng(10) > 5)) {
                    for (auto &pin : cursor_node.pins) {
                        if (!process_sitepin(net_idx, pin, true, /*do_bind*/ false))
                            continue;
                        if (pin.pin == id_CLK_IN)
                            vcc_allowed = true;
                        dst_pin = pin;
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
                    if (node2net.count(cursor_pip.dst_node) || visited_fwd.count(cursor_pip.dst_node) || is_routethru_blocked(cursor_pip))
                        continue;
                    visited_fwd[cursor_pip.dst_node] = pip_dh;
                    queue_fwd.push(cursor_pip.dst_node);
                }
            }

            if (fwd_endpoint == -1)
                continue; // no endpoint reached

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
                if (vcc_allowed && (rng.rng(10) > 3) && process_const_vcc(net_idx, cursor)) { // can tie to a constant
                    bwd_startpoint = cursor;
                    break;
                }
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
                    if (node2net.count(cursor_pip.src_node) || visited_bwd.count(cursor_pip.src_node) || is_routethru_blocked(cursor_pip))
                        continue;
                    visited_bwd[cursor_pip.src_node] = pip_uh;
                    queue_bwd.push(cursor_pip.src_node);
                }
            }

            if (bwd_startpoint == -1)
                continue; // no startpoint found

            log_verbose("    success (%d fwd %d bwd)!\n", fwd_iter, bwd_iter);

            // do bind
            process_sitepin(net_idx, dst_pin, true, /*do_bind*/ true);
            used_pips.insert(pip);
            node2net[src_node] = net_idx;
            node2net[dst_node] = net_idx;
            block_sites(pip_data);
            std::vector<index_t> fwd_nodes, bwd_nodes;
            fwd_cursor = fwd_endpoint;
            log_verbose("    fwd path:\n");
            while (fwd_cursor != dst_node) {
                fwd_nodes.push_back(fwd_cursor);
                if (verbose_flag)
                    log_verbose("        %s\n", node_name(fwd_cursor));
                node2net[fwd_cursor] = net_idx;
                auto &fwd_pip = graph.pips.at(visited_fwd.at(fwd_cursor));
                block_sites(fwd_pip);
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
                block_sites(bwd_pip);
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
        return false;
    }

    struct CellInst {
        int cell_idx = -1;
        IdString cell_type;
        dict<IdString, int> pin2net;
    };
    dict<std::pair<TileKey, IdString>, CellInst> cells;

    bool add_net_pin(int net, TileKey site, IdString bel, IdString cell_type, IdString cell_pin, bool do_bind) {
#if 0
        auto site_str = site.str(&ctx);
        log_verbose("adding pin %s/%s/%s on net %d\n", site_str.c_str(), bel.c_str(&ctx), cell_pin.c_str(&ctx), net);
#endif
        if (routethru_sites.count(site))
            return false; // don't place cells in the same site as routethru pips
        auto key = std::make_pair(site, bel);
        if (!do_bind) {
            if (cells.count(key) && cells.at(key).pin2net.count(cell_pin)) {
                int pin_net = cells.at(key).pin2net.at(cell_pin);
                // only need to skip if the net the pin is supposedly bound to exists
                // TODO: probably shouldn't bind nets to pins until routing actually succeeds...
                if (net2route.count(pin_net) || pin_net == net)
                    return false;
            }
            return true;
        }
        auto &cell_inst = cells[key];
        if (cell_inst.cell_type == IdString()) {
            cell_inst.cell_type = cell_type;
            cell_inst.cell_idx = int(cells.size());
        } else {
            MEOW_ASSERT(cell_inst.cell_type == cell_type);
        }
        if (cell_inst.pin2net.count(cell_pin)) {
            int pin_net = cell_inst.pin2net.at(cell_pin);
            // only need to skip if the net the pin is supposedly bound to exists
            // TODO: probably shouldn't bind nets to pins until routing actually succeeds...
            if (net2route.count(pin_net) || pin_net == net)
                return false;
        }
        cell_inst.pin2net[cell_pin] = net;
        used_sites.insert(site);
        return true;
    }

    bool process_const_vcc(int net, index_t node) {
        auto &node_data = graph.nodes.at(node);
        const std::string &name = node_data.name.str(&ctx);
        bool is_vcc = name.starts_with("VCC_WIRE");
        if (!is_vcc)
            return false;
        auto node_key = std::make_pair(node_data.tile, node_data.name);
        if (cells.count(node_key))
            return false;
        auto &cell_inst = cells[node_key];
        cell_inst.cell_type = id_VCC;
        cell_inst.cell_idx = int(cells.size());
        cell_inst.pin2net[id_P] = net;
        return true;
    }

    const dict<std::string, std::string> ps8_clkmap = {
        {"PL_CLK0", "PLCLK[0]"},
        {"PL_CLK1", "PLCLK[1]"},
        {"PL_CLK2", "PLCLK[2]"},
        {"PL_CLK3", "PLCLK[3]"},
        {"FMIO_GEM0_FIFO_TX_CLK_TO_PL_BUFG", "FMIOGEM0FIFOTXCLKTOPLBUFG"},
        {"FMIO_GEM1_FIFO_TX_CLK_TO_PL_BUFG", "FMIOGEM1FIFOTXCLKTOPLBUFG"},
        {"FMIO_GEM2_FIFO_TX_CLK_TO_PL_BUFG", "FMIOGEM2FIFOTXCLKTOPLBUFG"},
        {"FMIO_GEM3_FIFO_TX_CLK_TO_PL_BUFG", "FMIOGEM3FIFOTXCLKTOPLBUFG"},
        {"FMIO_GEM0_FIFO_RX_CLK_TO_PL_BUFG", "FMIOGEM0FIFORXCLKTOPLBUFG"},
        {"FMIO_GEM1_FIFO_RX_CLK_TO_PL_BUFG", "FMIOGEM1FIFORXCLKTOPLBUFG"},
        {"FMIO_GEM2_FIFO_RX_CLK_TO_PL_BUFG", "FMIOGEM2FIFORXCLKTOPLBUFG"},
        {"FMIO_GEM3_FIFO_RX_CLK_TO_PL_BUFG", "FMIOGEM3FIFORXCLKTOPLBUFG"},
        {"FMIO_GEM_TSU_CLK_TO_PL_BUFG", "FMIOGEMTSUCLKTOPLBUFG"},
        {"DP_AUDIO_REF_CLK", "DPAUDIOREFCLK"},
        {"DP_VIDEO_REF_CLK", "DPVIDEOREFCLK"},
    };

    bool process_sitepin(int net, SitePin pin, bool is_input, bool do_bind = true) {
        TileKey site = pin.site;
        if (site.prefix.in(id_BUFGCE, id_BUFGCE_DIV, id_BUFG_PS/*, id_BUFG_GT*/)) {
            IdString bel = (site.prefix == id_BUFGCE) ? id_BUFCE : site.prefix;
            if (pin.pin == id_CE_PRE_OPTINV && is_input && site.prefix != id_BUFG_GT)
                return add_net_pin(net, site, bel, site.prefix, id_CE, do_bind);
            if (pin.pin == id_CLK_IN && is_input)
                return add_net_pin(net, site, bel, site.prefix, id_I, do_bind);
            if (pin.pin == id_CLK_OUT && !is_input)
                return add_net_pin(net, site, bel, site.prefix, id_O, do_bind);
        } else if (site.prefix == id_BUFGCE_HDIO) {
            if (pin.pin == id_CLK_IN && is_input)
                return add_net_pin(net, site, id_BUFCE, id_BUFGCE, id_I, do_bind);
            if (pin.pin == id_CLK_OUT && !is_input)
                return add_net_pin(net, site, id_BUFCE, id_BUFGCE, id_O, do_bind);
        } else if (site.prefix == id_BUFGCTRL) {
            if (pin.pin == id_CLK_I0 && is_input)
                return add_net_pin(net, site, id_BUFGCTRL, id_BUFGCTRL, id_I0, do_bind);
            if (pin.pin == id_CLK_I1 && is_input)
                return add_net_pin(net, site, id_BUFGCTRL, id_BUFGCTRL, id_I1, do_bind);
            if (pin.pin == id_CLK_OUT && !is_input)
                return add_net_pin(net, site, id_BUFGCTRL, id_BUFGCTRL, id_O, do_bind);
        } else if ((site.prefix == id_BUFCE_ROW || site.prefix == id_BUFCE_ROW_FSR) && pin.pin == id_CLK_OUT && !is_input) {
            return add_net_pin(net, site, id_BUFCE, id_BUFCE_ROW, id_O, do_bind);
        } else if (site.prefix == id_BUFCE_LEAF) {
            if (pin.pin == id_CLK_IN && is_input)
                return add_net_pin(net, site, id_BUFCE, id_BUFCE_LEAF, id_I, do_bind);
            else if (pin.pin == id_CE_INT && is_input)
                return add_net_pin(net, site, id_BUFCE, id_BUFCE_LEAF, id_CE, do_bind);
            return false; // return log churn
        } else if (site.prefix == id_SLICE) {
            if ((pin.pin == id_CLK1 || pin.pin == id_CLK2) && is_input) {
                return add_net_pin(net, site, (pin.pin == id_CLK2) ? id_EFF : id_AFF, id_FDRE, id_C, do_bind);
            } else if ((pin.pin == id_SRST1 || pin.pin == id_SRST2) && is_input) {
                return add_net_pin(net, site, (pin.pin == id_SRST2) ? id_EFF : id_AFF, id_FDRE, id_R, do_bind);
            } else if ((pin.pin == id_CKEN1 || pin.pin == id_CKEN3) && is_input) {
                return add_net_pin(net, site, (pin.pin == id_CKEN3) ? id_EFF : id_AFF, id_FDRE, id_CE, do_bind);
            } else if ((pin.pin == id_CKEN2 || pin.pin == id_CKEN4) && is_input) {
                return add_net_pin(net, site, (pin.pin == id_CKEN4) ? id_EFF2 : id_AFF2, id_FDRE, id_CE, do_bind);
            } else if (!is_input) {
                const std::string &pin_str = pin.pin.str(&ctx);
                if (pin_str.size() == 3 && pin_str.substr(1) == "_O")
                    return add_net_pin(net, site, ctx.id(stringf("%c6LUT", pin_str.at(0))), id_LUT1, id_O, do_bind);
                if (pin_str.size() >= 2 && pin_str.at(1) == 'Q')
                    return add_net_pin(net, site, ctx.id(stringf("%cFF%s", pin_str.at(0), pin_str.c_str() + 2)), id_FDRE, id_Q, do_bind);
            }
        } else if (site.prefix == id_MMCM) {
            if (is_input ? pin.pin.in(id_CLKIN1, id_CLKIN2, id_CLKFBIN) :
                    pin.pin.in(id_CLKFBOUT, id_CLKFBOUTB, id_CLKOUT0, id_CLKOUT0B, id_CLKOUT1, id_CLKOUT1B, id_CLKOUT2, id_CLKOUT2B,
                        id_CLKOUT3, id_CLKOUT3B, id_CLKOUT4, id_CLKOUT5, id_CLKOUT6))
                return add_net_pin(net, site, id_MMCM, id_MMCME4_ADV, pin.pin, do_bind);
        } else if (site.prefix == id_PLL) {
            if (is_input ? pin.pin.in(id_CLKIN, id_CLKFBIN) :
                    pin.pin.in(id_CLKFBOUT, id_CLKOUT0, id_CLKOUT0B, id_CLKOUT1, id_CLKOUT1B, id_CLKOUTPHY))
                return add_net_pin(net, site, id_PLL, id_PLLE4_ADV, pin.pin, do_bind);
        } else if (site.prefix == id_BITSLICE_RX_TX) {
            if (is_input && pin.pin.in(id_RX_CLK, id_TX_CLK))
                return add_net_pin(net, site, id_RXTX_BITSLICE, id_RXTX_BITSLICE, pin.pin, do_bind);
            return false; // return log churn
        } else if (site.prefix == id_IOB && pin.pin == id_I && !is_input) {
            return add_net_pin(net, site, IdString(), id_IBUF, id_O, do_bind);
        } else if (site.prefix == id_PS8) {
            if (!is_input && ps8_clkmap.count(pin.pin.str(&ctx))) {
                return add_net_pin(net, site, id_PS8, id_PS8, ctx.id(ps8_clkmap.at(pin.pin.str(&ctx))), do_bind);
            }
            return false;
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
        int next_cell_idx = int(cells.size());
        for (auto &cell : cells) {
            int idx = cell.second.cell_idx;
            out << "set c [create_cell -reference " << cell.second.cell_type.c_str(&ctx) << " c" << idx << "]" << std::endl;
            if (!cell.second.cell_type.in(id_VCC, id_GND)) {
                if (cell.first.second == IdString())
                    out << "place_cell $c " << cell.first.first.str(&ctx) << std::endl;
                else
                    out << "place_cell $c " << cell.first.first.str(&ctx) << "/" << cell.first.second.c_str(&ctx) << std::endl;
                out << "set_property IS_LOC_FIXED 1 $c" << std::endl;
                if (cell.first.second != IdString())
                    out << "set_property IS_BEL_FIXED 1 $c" << std::endl;
            }
            out << "set_property keep 1 $c" << std::endl;
            out << "set_property dont_touch 1 $c" << std::endl;
            out << std::endl;
            for (auto pin : cell.second.pin2net) {
                net2pin[pin.second].emplace_back(cell.second.cell_idx, pin.first);
            }
            if (cell.second.cell_type == id_IBUF) {
                out << "set p [create_port -direction IN c" << idx << "_io]" << std::endl;
                out << "set n [create_net c" << idx << "_io]" << std::endl;
                out << "connect_net -net $n -objects {c" << idx << "_io c" << idx << "/I}" << std::endl;
            } else if (cell.second.cell_type == id_BUFG_GT) {
                // TODO: broken
                int sync_idx = next_cell_idx++;
                out << "set c [create_cell -reference BUFG_GT_SYNC c" << sync_idx << "]" << std::endl;
                auto sync_prefix =  cell.first.first;
                sync_prefix.prefix = id_BUFG_GT_SYNC;
                out << "place_cell $c " << sync_prefix.str(&ctx) << "/BUFG_GT_SYNC" << std::endl;
                out << "set_property keep 1 $c" << std::endl;
                out << "set_property dont_touch 1 $c" << std::endl;
                out << "set_property IS_LOC_FIXED 1 $c" << std::endl;
                out << "set_property IS_BEL_FIXED 1 $c" << std::endl;
                out << "set n [create_net c" << sync_idx << "_ce]" << std::endl;
                out << "connect_net -net $n -objects {c" << idx << "/CE c" << sync_idx << "/CESYNC}" << std::endl;
                out << "set n [create_net c" << sync_idx << "_clr]" << std::endl;
                out << "connect_net -net $n -objects {c" << idx << "/CLR c" << sync_idx << "/CLRSYNC}" << std::endl;
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
        used_sites.clear();
        routethru_sites.clear();
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
        if (args.named.count("cov")) {
            std::ofstream out(args.named.at("cov").at(0));
            for (auto cov : covered_ttpips) {
                out << cov.first.tile_type.c_str(&ctx) << " " << cov.first.pip_name.c_str(&ctx) << " " << cov.second << std::endl;
            }
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
        for (int design = 1; design <= (args.named.count("num-designs") ? std::stoi(args.named.at("num-designs").at(0)) : 20); design++) {
            setup_design();
            for (int i = 0; i < 50000; i++) {
                net++;
                auto try_pip = try_pips.at(i % int(try_pips.size()));
                do_route(net, try_pip, covered_ttpips.count(try_pip) ? (covered_ttpips.at(try_pip) < 25) : true);
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
    parser.add_opt("cov", 1, "write detailed coverage data");

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
