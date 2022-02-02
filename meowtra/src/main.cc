#include "bitstream.h"
#include "log.h"
#include "cmdline.h"

#include <fstream>
#include <iostream>

USING_MEOW_NAMESPACE;

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

int main(int argc, char *argv[]) {
    auto top_help = [&]() {
        std::cerr << "Usage: ";
        std::cerr << argv[0] << " <unpack|pack> <options>" << std::endl;
    };
    if (argc < 2) {
        top_help();
        return 1;
    }
    std::string subcommand(argv[1]);
    if (subcommand == "unpack") {
        return subcmd_unpack(argc, (const char**)argv);
    } else if (subcommand == "pack") {
        MEOW_ASSERT_FALSE("unimplemented!");
    } else {
        top_help();
        return 1;
    }
    return 0;
}
