#include "tools.h"
#include "log.h"
#include "datafile.h"

#include <iostream>

USING_MEOW_NAMESPACE;

int main(int argc, char *argv[]) {
    auto top_help = [&]() {
        std::cerr << "Usage: ";
        std::cerr << argv[0] << " <unpack|pack|correlate|fuzztools> <options>" << std::endl;
    };
    if (argc < 2) {
        top_help();
        return 1;
    }
    std::string subcommand(argv[1]);
    if (subcommand == "unpack") {
        return subcmd_unpack(argc, (const char**)argv);
    } else if (subcommand == "correlate") {
        return subcmd_correlate(argc, (const char**)argv);
    } else if (subcommand == "fuzztools") {
        return subcmd_fuzztools(argc, (const char**)argv);
    } else if (subcommand == "pack") {
        MEOW_ASSERT_FALSE("unimplemented!");
    } else {
        top_help();
        return 1;
    }
    return 0;
}
