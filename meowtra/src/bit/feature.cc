#include "feature.h"
#include "context.h"

MEOW_NAMESPACE_BEGIN

Feature Feature::parse(Context *ctx, std::string_view view) {
    auto br_pos = view.rfind('[');
    if (br_pos == std::string_view::npos) {
        return Feature(ctx->id(std::string(view)));
    } else {
        IdString base = ctx->id(std::string(view.substr(0, br_pos)));
        index_t idx = parse_i32(view.substr(br_pos + 1, view.size() - (br_pos + 2)));
        return Feature(base, idx);
    }
}

void Feature::write(Context *ctx, std::ostream &out) const {
    out << base.str(ctx);
    if (bit != -1)
        out << '[' << bit << ']';
}

TileFeatures TileFeatures::parse(Context *ctx, line_range lines) {
    TileFeatures result;
    for (auto line : lines) {
        auto i = line.begin();
        if (i == line.end())
            continue;
        auto [tile, feature] = split_view(*i, '.');
        result.tiles[TileKey::parse(ctx, tile)].insert(Feature::parse(ctx, feature));
    }
    return result;
}

void TileFeatures::write(Context *ctx, std::ostream &out) const {
    for (const auto &entry : tiles) {
        if (entry.second.empty())
            continue;
        auto prefix = entry.first.str(ctx);
        for (auto feat : entry.second) {
            out << prefix << ".";
            feat.write(ctx, out);
            out << std::endl;
        }
        out << std::endl;
    }
}

MEOW_NAMESPACE_END
