#include "nodegraph.h"
#include "datafile.h"
#include "context.h"
#include "log.h"

#include <fstream>

MEOW_NAMESPACE_BEGIN

NodeGraph NodeGraph::parse(Context *ctx, const std::string &file) {
    NodeGraph result;
    idict<std::pair<TileKey, IdString>> nodes_by_name;
    auto get_node = [&](std::string_view name) {
        auto spl = split_view(name, '/');
        auto key = std::make_pair(TileKey::parse(ctx, spl.first), ctx->id(std::string(spl.second)));
        index_t idx = nodes_by_name(key);
        MEOW_ASSERT(idx <= index_t(result.nodes.size()));
        if (idx == index_t(result.nodes.size())) {
            result.nodes.emplace_back(key.first, key.second);
        }
        return idx;
    };
    std::ifstream in_graph(file);
    std::string graph_buf(std::istreambuf_iterator<char>(in_graph), {});
    for (auto line : lines(graph_buf)) {
        auto iter = line.begin();
        if (iter == line.end())
            continue;
        auto mode = *iter++;
        if (mode == "pip" || mode == "extpip") {
            index_t pip_idx = index_t(result.pips.size());
            result.pips.emplace_back();
            auto &pip_data = result.pips.back();
            if (mode == "extpip") {
                pip_data.tile = TileKey::parse(ctx, *iter++);
                pip_data.bidi = false;
            } else {
                auto spl = split_view(*iter++, '/');
                pip_data.tile = TileKey::parse(ctx, spl.first);
                pip_data.pip_name = ctx->id(std::string(spl.second));
                pip_data.bidi = (mode == "pip") ? (parse_u32(*iter++) == 0) : false;
            }
            if (iter == line.end()) {
                result.pips.pop_back();
                continue; // missing node
            }
            pip_data.src_node = get_node(*iter++);
            if (iter == line.end()) {
                result.pips.pop_back();
                continue; // missing node
            }
            pip_data.dst_node = get_node(*iter++);
            pip_data.is_rclk = (mode == "pip");
            result.nodes.at(pip_data.src_node).pips_downhill.push_back(pip_idx);
            result.nodes.at(pip_data.dst_node).pips_uphill.push_back(pip_idx);
        } else if (mode == "pin" || mode == "extpin") {
            auto split_pin = split_view(*iter++, '/');
            TileKey site_key = TileKey::parse(ctx, split_pin.first);
            IdString pin_name = ctx->id(std::string(split_pin.second));
            if (iter == line.end())
                continue; // missing node
            result.nodes.at(get_node(*iter++)).pins.emplace_back(site_key, pin_name);
        } else {
            std::string mode_str(mode);
            log_error("unexpected clkgraph entry '%s'\n", mode_str.c_str());
        }
    }
    log_verbose("Parsed %d nodes and %d pips.\n",
        int(result.nodes.size()), int(result.pips.size()));
    return result;
};

MEOW_NAMESPACE_END
