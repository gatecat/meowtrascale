#include "tile.h"
#include "log.h"
#include "context.h"
#include "database.h"
#include "bitstream.h"

MEOW_NAMESPACE_BEGIN

std::string TileKey::str(const Context *ctx) const {
    return stringf("%s_X%dY%d", prefix.c_str(ctx), x, y);
}

TileGrid frames_to_tiles(Context *ctx, const BitstreamFrames &frames) {
    TileGrid result;
    auto regions = get_tile_regions(ctx, *frames.dev);
    for (const auto &r : regions) {
        std::vector<TileData> tiles(r.num_tiles);
        for (auto &t : tiles) {
            t.tile_type = r.prefix;
            t.bits = r.tile_height * 48;
            t.frames = r.tile_frames;
        }
        for (uint32_t f = 0; f < r.tile_frames; f++) {
            FrameKey k{r.slr, f + r.start_frame};
            if (!frames.frame_data.count(k)) {
                continue;
            }
            auto &data = frames.frame_data.at(k);
            index_t bit = r.start_bit;
            for (index_t i = 0; i < r.num_tiles; i++) {
                // TODO: speedup ?
                auto &t = tiles.at(i);
                for (index_t j = 0; j < (r.tile_height * 48); j++) {
                    if ((data.get(bit / 32U) >> (bit % 32U)) & 0x1) {
                        t.set_bits.insert(f * t.bits + j);
                    }
                    bit++;
                    if (bit == 1440)
                        bit = 1488; //skip ECC
                }
            }
        }
        for (index_t i = 0; i < r.num_tiles; i++) {
            TileKey key{.prefix=r.prefix, .x=r.tile_x, .y=int16_t(r.tile_y0+i)};
            result.tiles[key] = std::move(tiles.at(i));
        }
    }
    return result;
}

MEOW_NAMESPACE_END
