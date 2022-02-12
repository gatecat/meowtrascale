#include "tools.h"
#include "bitstream.h"
#include "tile.h"
#include "context.h"
#include "cmdline.h"
#include "log.h"
#include "database.h"
#include "constids.h"

#include <fstream>

MEOW_NAMESPACE_BEGIN

namespace {
void dump_frame_addrs(const std::vector<BitstreamPacket> &packets, std::ostream &out) {
    for (auto &packet : packets) {
        if (packet.reg != BitstreamPacket::FAR)
            continue;
        out << stringf("%d %08x\n", int(packet.slr), packet.payload.get(0));
    }
}
void dump_tile_bits(Context *ctx, TileGrid grid, std::ostream &out, bool skip_default_logic = true) {
    static const pool<index_t> empty_logic_tile = {0, 8, 16, 24, 32, 40, 192, 200, 208, 216, 224, 232, 384, 392, 416, 424};

    for (auto &t : grid.tiles) {
        if (t.second.set_bits.empty())
            continue;
        if (skip_default_logic && t.first.prefix.in(id_CLEL_L, id_CLEL_R, id_CLEM, id_CLEM_R)) {
            if (t.second.set_bits == empty_logic_tile)
                continue;
        }
        out << ".tile " << t.first.str(ctx) << std::endl;
        for (index_t b : t.second.set_bits)
            out << stringf("%d_%d\n", b / t.second.bits, b % t.second.bits);
    }
}
}

int subcmd_unpack(int argc, const char *argv[]) {
    CmdlineParser parser;
    parser.add_opt("v", 0, "verbose output");
    parser.add_opt("frame-addrs", 0, "dump frame addresses only (for bootstrapping)");

    parser.add_positional("bitstream", false, "input bitstream file");
    parser.add_positional("result", true, "output results file");
    CmdlineResult result;
    if (!parser.parse(argc, argv, 2, std::cerr, result))
        return 1;
    if (result.named.count("v"))
        verbose_flag = true;
    std::ifstream in(result.positional.at(0), std::ios::binary);
    std::ofstream *out_file = nullptr;
    if (int(result.positional.size()) >= 2)
        out_file = new std::ofstream(result.positional.at(1));
    auto &out_stream = out_file ? *out_file : std::cout;

    auto bit = RawBitstream::read(in);
    {
        auto packets = bit.packetise();
        if (result.named.count("frame-addrs")) {
            dump_frame_addrs(packets, out_stream);
        } else {
            auto frames = packets_to_frames(packets);
            log_info("device: %s\n", frames.dev->name.c_str());
            Context ctx;
            auto grid = frames_to_tiles(&ctx, frames);
            dump_tile_bits(&ctx, grid, out_stream);
        }
    }
    return 0;
}

MEOW_NAMESPACE_END
