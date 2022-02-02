#ifndef CMDLINE_H
#define CMDLINE_H

#include "preface.h"

#include <unordered_map>
#include <iostream>
#include <vector>

MEOW_NAMESPACE_BEGIN

struct CmdlineResult {
    std::unordered_map<std::string, std::vector<std::string>> named;
    std::vector<std::string> positional;
};

struct CmdlineParser {
    std::unordered_map<std::string, std::pair<int, std::string>> named_opts;
    std::vector<std::tuple<std::string, bool, std::string>> pos_opts;

    void add_opt(const std::string &name, int arg_count, const std::string &help);
    void add_positional(const std::string &name, bool optional, const std::string &help);

    void print_help(const char *argv[], int arg_offset, std::ostream &out);

    bool parse(int argc, const char *argv[], int arg_offset, std::ostream &out, CmdlineResult &result);
};

MEOW_NAMESPACE_END

#endif
