#ifndef DATABASE_H
#define DATABASE_H

#include <string>
#include <vector>

#include "preface.h"
#include "idstring.h"

MEOW_NAMESPACE_BEGIN

const std::string &get_db_root();

struct Device {
    std::string family;
    std::string name;
    uint32_t idcode;
    // ...
};

extern const std::vector<Device> all_devices;

const Device *device_by_idcode(uint32_t idcode);

struct FrameRange {
    uint32_t slr;
    uint32_t begin;
    uint32_t count;
    bool operator<(const FrameRange &f) const {
        return slr < f.slr || (slr == f.slr && begin < f.begin);
    }
    bool operator==(const FrameRange &f) const {
        return slr == f.slr && begin == f.begin;
    }
};

std::vector<FrameRange> get_device_frames(const Device &dev);

struct TileRegion {
    IdString prefix;
    int16_t tile_x;
    int16_t tile_y0;
    uint16_t slr;
    uint32_t start_frame;
    uint32_t start_bit;
    uint16_t tile_height;
    uint16_t tile_frames;
    uint16_t num_tiles;
};

std::vector<TileRegion> get_tile_regions(Context *ctx, const Device &dev);

MEOW_NAMESPACE_END

#endif
