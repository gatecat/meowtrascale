#ifndef NODEGRAPH_H
#define NODEGRAPH_H

#include "preface.h"
#include "tile_key.h"
#include "idstring.h"

MEOW_NAMESPACE_BEGIN

struct Pip {
    TileKey tile;
    IdString pip_name;
    index_t src_node;
    index_t dst_node;
    bool bidi = false;
    bool is_rclk = false;
};

struct SitePin {
    SitePin(TileKey site, IdString pin) : site(site), pin(pin) {}
    TileKey site;
    IdString pin;
};

struct Node {
    Node(TileKey tile, IdString name) : tile(tile), name(name) {};
    TileKey tile;
    IdString name;
    std::vector<index_t> pips_uphill;
    std::vector<index_t> pips_downhill;
    std::vector<SitePin> pins;
};

struct Context;

struct NodeGraph {
    static NodeGraph parse(Context *ctx, const std::string &file);
    std::vector<Node> nodes;
    std::vector<Pip> pips;
};

MEOW_NAMESPACE_END

#endif
