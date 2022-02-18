#include "tools.h"
#include "bitstream.h"
#include "tile.h"
#include "context.h"
#include "cmdline.h"
#include "log.h"
#include "database.h"
#include "constids.h"
#include "feature.h"

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
        log_info("loading %d bitstream+feature pairs\n", int(file_prefices.size()));
        tile_bits.resize(file_prefices.size());
        tile_feats.resize(file_prefices.size());
        std::vector<std::thread> threads;
        threads.reserve(file_prefices.size());
        for (index_t i = 0; i < index_t(file_prefices.size()); i++)
            threads.emplace_back([this, i]() { this->worker(i); });
        for (auto &th : threads)
            th.join();
    }

    void run() {
        find_files();
        parse_files();
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
