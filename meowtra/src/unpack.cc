#include "tools.h"
#include "bitstream.h"
#include "cmdline.h"
#include "log.h"

#include <fstream>

MEOW_NAMESPACE_BEGIN

int subcmd_unpack(int argc, const char *argv[]) {
    CmdlineParser parser;
    parser.add_opt("v", 0, "verbose output");
    parser.add_positional("bitstream", false, "input bitstream file");
    parser.add_positional("result", true, "output results file");
    CmdlineResult result;
    if (!parser.parse(argc, argv, 2, std::cerr, result))
        return 1;
    if (result.named.count("v"))
        verbose_flag = true;
    std::ifstream in(result.positional.at(0), std::ios::binary);
    auto bit = RawBitstream::read(in);
    {
        auto packets = bit.packetise();
    }
    return 0;
}

MEOW_NAMESPACE_END
