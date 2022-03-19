#include "split_sites.h"
#include "constids.h"
#include "log.h"
#include "context.h"

MEOW_NAMESPACE_BEGIN

std::vector<SplitSite> split_sites(Context *ctx, IdString tile_type, const pool<index_t> &set_bits, const pool<Feature> &features) {
    std::vector<SplitSite> result;
    int tile_height = 1;
    auto do_split = [&](IdString site_type, int dx, int dy, int start_frame, int total_frames, int start_word, int total_words) {
        const std::string prefix = stringf("%s_X%dY%d.", site_type.c_str(ctx), dx, dy);
        result.emplace_back();
        auto &s = result.back();
        s.site_type = site_type;
        for (auto f : features) {
            const std::string &f_name = f.base.str(ctx);
            if (f_name.starts_with(prefix)) {
                s.set_features.emplace(ctx->id(f_name.substr(prefix.size())), f.bit);
            }
        }
        for (auto b : set_bits) {
            int frame = b / (tile_height * 48);
            int framebit = b % (tile_height * 48);
            int word = framebit / 48;
            if (frame < start_frame || frame >= (start_frame + total_frames))
                continue;
            if (word < start_word || word >= (start_word + total_words))
                continue;
            int site_bit = ((frame - start_frame) * tile_height * 48) + ((word - start_word) * 48) + (b % 48);
            s.set_bits.insert(site_bit);
        }
    };
    if (tile_type == id_HPIO_L) {
        tile_height = 30;
        for (int y = 0; y <= 25; y++) {
            if (y == 12 || y == 25) {
                do_split(id_HPIOB_SNGL, 0, y, (y == 12) ? 2 : 0, 2, 16, 2);
            } else {
                int bit_y = (y >= 12) ? (y + 3) : y;
                IdString site_type = (y % 2) == ((y < 12) ? 0 : 1) ? id_HPIOB_M : id_HPIOB_S;
                do_split(site_type, 0, y, ((bit_y % 4) < 2) ? 2 : 0, 2, 2 + 2 * (2 * (bit_y / 4) + (bit_y % 2)), 2);
            }
        }
    }
    return result;
}

MEOW_NAMESPACE_END
