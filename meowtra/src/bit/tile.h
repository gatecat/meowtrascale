#ifndef TILE_H
#define TILE_H

#include "preface.h"
#include "idstring.h"
#include "hashlib.h"

#include <vector>

MEOW_NAMESPACE_BEGIN

struct TileKey {
    IdString prefix;
    int16_t x;
    int16_t y;

    bool operator==(const TileKey &other) const { return prefix == other.prefix && x == other.x && y == other.y; }
    bool operator!=(const TileKey &other) const { return prefix != other.prefix || x != other.x || y != other.y; }
    unsigned hash() const { return mkhash(prefix.hash(), mkhash(x, y)); }
    std::string str(const Context *ctx) const;
};

struct TileData {
    IdString tile_type;
    index_t frames, bits;
    pool<index_t> set_bits;
};

struct TileGrid {
    dict<TileKey, TileData> tiles;
};

struct BitstreamFrames;

TileGrid frames_to_tiles(Context *ctx, const BitstreamFrames &frames);

MEOW_NAMESPACE_END

#endif

