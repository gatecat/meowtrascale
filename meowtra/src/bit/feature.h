#ifndef FEATURE_H
#define FEATURE_H

#include "preface.h"
#include "idstring.h"
#include "hashlib.h"
#include "tile_key.h"
#include "datafile.h"

#include <string_view>
#include <iostream>

MEOW_NAMESPACE_BEGIN

struct Context;

struct Feature {
    explicit Feature(IdString base, index_t bit = -1) : base(base), bit(bit) {};
    IdString base;
    index_t bit;
    bool operator==(const Feature &other) const { return base == other.base && bit == other.bit; }
    bool operator!=(const Feature &other) const { return base != other.base || bit != other.bit; }
    unsigned hash() const {
        return mkhash(base.hash(), bit);
    }
    static Feature parse(Context *ctx, std::string_view view);
    void write(Context *ctx, std::ostream &out) const;
};

struct TileFeatures {
    dict<TileKey, pool<Feature>> tiles;
    static TileFeatures parse(Context *ctx, line_range lines);
    void write(Context *ctx, std::ostream &out) const;
};

MEOW_NAMESPACE_END

#endif

