#include "tools.h"
#include "context.h"
#include "cmdline.h"
#include "log.h"
#include "database.h"
#include "constids.h"
#include "nodegraph.h"

#include <fstream>

MEOW_NAMESPACE_BEGIN

int subcmd_fuzztools(int argc, const char *argv[]) {
    CmdlineParser parser;
    parser.add_opt("v", 0, "verbose output");
    parser.add_opt("nodegraph", 1, "path to nodegraph file");

    CmdlineResult result;
    if (!parser.parse(argc, argv, 2, std::cerr, result))
        return 1;

    if (result.named.count("v"))
        verbose_flag = true;

    Context ctx;
    NodeGraph g = NodeGraph::parse(&ctx, result.named.at("nodegraph").at(0));

    return 0;
}

MEOW_NAMESPACE_END
