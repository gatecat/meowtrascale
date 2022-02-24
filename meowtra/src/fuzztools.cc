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

    struct TileTypeData {
        IdString name;
        index_t tried_pips = 0;
        index_t success_pips = 0;
        index_t pip_count = 0;
        idict<TileKey> tile_insts;
        std::vector<std::vector<index_t>> tile_pips;
    };

    std::vector<TileTypeData> tile_types;
    idict<IdString> tile_type_keys;

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
            type_data.tile_pips.at(inst_idx).push_back(i);
        }
        for (auto &tt : tile_types) {
            for (auto &inst : tt.tile_pips)
                tt.pip_count = std::max(tt.pip_count, index_t(inst.size()));
        }
    }

    index_t get_next_tiletype() {
        index_t next = -1;
        double lowest_coverage = std::numeric_limits<double>::max();
        for (index_t i = 0; i < index_t(tile_types.size()); i++) {
            auto &tt = tile_types.at(i);
            if (tt.pip_count == 0)
                continue;
            double coverage = double(tt.tried_pips) / double(tt.pip_count);
            if (coverage < lowest_coverage || ((coverage == lowest_coverage) && rng.rng(2))) {
                next = i;
                lowest_coverage = coverage;
            }
        }
        MEOW_ASSERT(next != -1);
        return next;
    }

    dict<index_t, index_t> node2net; 
    pool<index_t> used_pips;

    index_t get_next_pip(TileTypeData &tt) {
        // up to 5 tries
        for (index_t i = 0; i < 5; i++) {
            auto &inst = rng.choice(tt.tile_pips);
            if (inst.empty())
                continue;
            index_t pip = rng.choice(inst);
            if (used_pips.count(pip))
                continue;
            auto &pip_data = graph.pips.at(pip);
            if (node2net.count(pip_data.src_node))
                continue;
            if (node2net.count(pip_data.dst_node))
                continue;
            return pip;
        }
        return -1;
    }

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

    bool do_route(int net_idx) {
        index_t tti = get_next_tiletype();
        auto &tt = tile_types.at(tti);
        ++tt.tried_pips;
        index_t pip = get_next_pip(tt);
        if (pip == -1)
            return false;
        auto &pip_data = graph.pips.at(pip);
        dict<index_t, index_t> visited_fwd, visited_bwd;
        if (verbose_flag)
            log_verbose("trying %s --> %s\n", node_name(pip_data.src_node), node_name(pip_data.dst_node));
        std::queue<index_t> queue_fwd, queue_bwd;
        visited_fwd[pip_data.dst_node] = pip;
        visited_fwd[pip_data.src_node] = -1; // disallow in fwd cone
        queue_fwd.push(pip_data.dst_node);

        int fwd_iter = 0;
        index_t fwd_endpoint = -1;
        while (!queue_fwd.empty() && fwd_iter++ < iter_limit) {
            index_t cursor = queue_fwd.front();
            queue_fwd.pop();
            auto &cursor_node = graph.nodes.at(cursor);
            if (!cursor_node.pins.empty()) {
                // TODO: check pin is actually usable and how we'd use it....
                fwd_endpoint = cursor;
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

        visited_bwd[pip_data.src_node] = pip;
        // disallow fwd cone in bwd cone
        index_t fwd_cursor = fwd_endpoint;
        while (fwd_cursor != pip_data.dst_node) {
            visited_bwd[fwd_cursor] = -1;
            auto &fwd_pip = graph.pips.at(visited_fwd.at(fwd_cursor));
            fwd_cursor = fwd_pip.src_node;
        }
        visited_bwd[pip_data.dst_node] = -1; // disallow in bwd cone
        queue_bwd.push(pip_data.src_node);

        int bwd_iter = 0;
        index_t bwd_startpoint = -1;
        while (!queue_bwd.empty() && bwd_iter++ < iter_limit) {
            index_t cursor = queue_bwd.front();
            queue_bwd.pop();
            auto &cursor_node = graph.nodes.at(cursor);
            if (!cursor_node.pins.empty()) {
                // TODO: check pin is actually usable and how we'd use it....
                bwd_startpoint = cursor;
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
        node2net[pip_data.src_node] = net_idx;
        node2net[pip_data.dst_node] = net_idx;
        fwd_cursor = fwd_endpoint;
        log_verbose("    fwd path:\n");
        while (fwd_cursor != pip_data.dst_node) {
            if (verbose_flag)
                log_verbose("        %s\n", node_name(fwd_cursor));
            node2net[fwd_cursor] = net_idx;
            auto &fwd_pip = graph.pips.at(visited_fwd.at(fwd_cursor));
            fwd_cursor = fwd_pip.src_node;
        }
        log_verbose("    bwd path:\n");
        index_t bwd_cursor = bwd_startpoint;
        while (bwd_cursor != pip_data.src_node) {
            if (verbose_flag)
                log_verbose("        %s\n", node_name(bwd_cursor));
            node2net[bwd_cursor] = net_idx;
            auto &bwd_pip = graph.pips.at(visited_bwd.at(bwd_cursor));
            bwd_cursor = bwd_pip.dst_node;
        }
        ++tt.success_pips;
        return true;
    }

    void print_coverage() {
        for (auto &tt : tile_types)
            log_info("%30s %6d %6d\n", tt.name.c_str(&ctx), tt.success_pips, tt.pip_count);
    }

    int operator()() {
        if (args.named.count("seed"))
            rng.rngseed(parse_u32(args.named.at("seed").at(0)));
        else
            rng.rngseed(1);
        setup_tiletypes();
        for (int i = 0; i < 50000; i++)
            do_route(i);
        print_coverage();
        return 0;
    }
};

int subcmd_fuzztools(int argc, const char *argv[]) {
    CmdlineParser parser;
    parser.add_opt("v", 0, "verbose output");
    parser.add_opt("nodegraph", 1, "path to nodegraph file");

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
