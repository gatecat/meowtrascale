#include "tools.h"
#include "bitstream.h"
#include "cmdline.h"
#include "log.h"

#include <fstream>

MEOW_NAMESPACE_BEGIN

namespace {
void dump_frame_adders(const std::vector<BitstreamPacket> &packets, std::ostream &out) {
    for (auto &packet : packets) {
        if (packet.reg != BitstreamPacket::FAR)
            continue;
        out << stringf("%d %08x\n", int(packet.slr), packet.payload.get(0));
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
        if (result.named.count("frame-addrs"))
            dump_frame_adders(packets, out_stream);
    }
    return 0;
}

MEOW_NAMESPACE_END
