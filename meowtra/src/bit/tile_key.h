#ifndef TILE_KEY_H
#define TILE_KEY_H

#include <string_view>

#include "preface.h"
#include "idstring.h"
#include "hashlib.h"

MEOW_NAMESPACE_BEGIN

struct TileKey {
    IdString prefix;
    int16_t x;
    int16_t y;

    bool operator==(const TileKey &other) const { return prefix == other.prefix && x == other.x && y == other.y; }
    bool operator!=(const TileKey &other) const { return prefix != other.prefix || x != other.x || y != other.y; }
    unsigned hash() const { return mkhash(prefix.hash(), mkhash(x, y)); }
    std::string str(const Context *ctx) const;

    static TileKey parse(Context *ctx, const std::string_view &s);
};

MEOW_NAMESPACE_END

#endif
