#include "tools.h"
#include "bitstream.h"
#include "tile.h"
#include "context.h"
#include "cmdline.h"
#include "log.h"
#include "database.h"
#include "constids.h"
#include "feature.h"
#include "specimen.h"

#include <fstream>
#include <thread>
#include <vector>
#include <filesystem>

MEOW_NAMESPACE_BEGIN

namespace {

struct CorrelateWorker {
    CorrelateWorker(CmdlineResult args) : args(args) {};
    CmdlineResult args;

    void find_files() {
        std::filesystem::path spec_dir(args.positional.at(0));
        for (auto entry : std::filesystem::directory_iterator(spec_dir)) {
            auto path = entry.path();
            if (path.extension() != ".features")
                continue;
            auto base = path.stem().string();
            if (!std::filesystem::exists(spec_dir / (base + ".bit")))
                continue;
            file_prefices.push_back(spec_dir / base);
        }
    }

    void worker(index_t i) {
        std::ifstream in_bit(file_prefices.at(i) + ".bit", std::ios::binary);
        auto bit = RawBitstream::read(in_bit);
        auto packets = bit.packetise();
        auto frames = packets_to_frames(packets);
        tile_bits.at(i) = frames_to_tiles(&ctx, frames);
        std::ifstream in_feat(file_prefices.at(i) + ".features");
        std::string feat_buf(std::istreambuf_iterator<char>(in_feat), {});
        tile_feats.at(i) = TileFeatures::parse(&ctx, lines(feat_buf));
    }

    void parse_files() {
        log_info("loading %d bitstream+feature pairs...\n", int(file_prefices.size()));
        tile_bits.resize(file_prefices.size());
        tile_feats.resize(file_prefices.size());
        std::vector<std::thread> threads;
        threads.reserve(file_prefices.size());
        for (index_t i = 0; i < index_t(file_prefices.size()); i++)
            threads.emplace_back([this, i]() { this->worker(i); });
        for (auto &th : threads)
            th.join();
    }

    void filter_tiles() {
        std::vector<std::string_view> tile_rules;
        if (args.named.count("tiles")) {
            std::string_view tr(args.named.at("tiles").at(0));
            while(true) {
                size_t pos = tr.find(',');
                if (pos == std::string_view::npos) {
                    tile_rules.push_back(tr);
                    break;
                } else {
                    tile_rules.push_back(tr.substr(0, pos));
                    tr = tr.substr(pos + 1);
                }
            }
        }
        pool<IdString> all_tiletypes;
        for (const auto &fs : tile_feats) {
            for (const auto &entry : fs.tiles) {
                all_tiletypes.insert(entry.first.prefix);
            }
        }
        if (tile_rules.empty()) { // include everything
            included_tiletypes = all_tiletypes;
        } else {
            for (auto tt : all_tiletypes) {
                const std::string &tt_str = tt.str(&ctx);
                for (auto rule : tile_rules) {
                    if (rule == tt_str) {
                        included_tiletypes.insert(tt);
                    } else if (rule.at(rule.size() - 1) == '*' && rule.substr(0, rule.size() - 1) == tt_str.substr(0, rule.size() - 1)) {
                        included_tiletypes.insert(tt);
                    }
                }
            }
        }
    }

    void filter_bits(const std::string &mode) {
        bool filter_cmt = (mode == "EXCL_CMT_DRP");
        for (auto &des : tile_bits) {
            for (auto &tile : des.tiles) {
                // exclude the DRP part of CMT tiles (PLL stuff)
                if (tile.first.prefix == id_CMT_L && filter_cmt) {
                    for (int i = 0; i < (4*60*48); i++)
                        tile.second.set_bits.erase(i);
                }
            }
        }
    }

    void do_correlate(IdString tt) {
        SpecimenGroup group;
        for (index_t i = 0; i < index_t(tile_bits.size()); i++) {
            auto &bits = tile_bits.at(i);
            auto &feats = tile_feats.at(i);
            for (auto &feat_tile : feats.tiles) {
                if (feat_tile.first.prefix != tt)
                    continue;
                if (!bits.tiles.count(feat_tile.first))
                    continue;
                auto &bit_tile = bits.tiles.at(feat_tile.first);
                group.tile_bits = bit_tile.bits;
                group.specs.emplace_back(feat_tile.second, bit_tile.set_bits);
            }
        }
        if (group.specs.empty())
            return;
        log_info("processing tile type %s...\n", tt.c_str(&ctx));
        std::cout << "*** " << tt.c_str(&ctx) << " ***" << std::endl;
        group.find_deps();
        group.solve(&ctx);
    }

    void run() {
        find_files();
        parse_files();
        filter_tiles();
        if (args.named.count("filter"))
            filter_bits(args.named.at("filter").at(0));
        for (auto tt : included_tiletypes)
            do_correlate(tt);
    }

    Context ctx;
    pool<IdString> included_tiletypes;
    std::vector<std::string> file_prefices;
    std::vector<TileGrid> tile_bits;
    std::vector<TileFeatures> tile_feats;
};

}

int subcmd_correlate(int argc, const char *argv[]) {
    CmdlineParser parser;
    parser.add_opt("v", 0, "verbose output");
    parser.add_opt("filter", 1, "filter mode");
    parser.add_opt("tiles", 1, "comma separated list of tile types");
    parser.add_opt("min-count", 1, "minimum number of samples for a feature");
    parser.add_positional("folder", false, "specimen folder");

    CmdlineResult result;
    if (!parser.parse(argc, argv, 2, std::cerr, result))
        return 1;

    CorrelateWorker worker(result);
    worker.run();

    return 0;
}

MEOW_NAMESPACE_END
