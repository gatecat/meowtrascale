#ifndef TILE_H
#define TILE_H

#include "preface.h"
#include "idstring.h"
#include "hashlib.h"
#include "tile_key.h"

#include <vector>

MEOW_NAMESPACE_BEGIN

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

