#include "cmdline.h"

MEOW_NAMESPACE_BEGIN

namespace {
void write_rpad(std::ostream &out, const std::string &str, int pad) {
    out << str;
    for (int i = int(str.size()); i < pad; i++) out << ' ';
}
};

void CmdlineParser::add_opt(const std::string &name, int arg_count, const std::string &help) {
    named_opts[name] = std::make_pair(arg_count, help);
}

void CmdlineParser::add_positional(const std::string &name, bool optional, const std::string &help) {
    pos_opts.emplace_back(name, optional, help);
}

void CmdlineParser::print_help(const char *argv[], int arg_offset, std::ostream &out) {
    out << "Usage:";
    for (int i = 0; i < arg_offset; i++)
        out << " " << argv[i];
    out << " [options]";
    for (auto &[name, optional, help] : pos_opts) {
        (void)help; // unused
        out << " " << (optional ? '[' : '<') << name << (optional ? ']' : '>');
    }
    out << std::endl << std::endl;
    int max_len = 5; // -help
    for (auto n : named_opts)
        max_len = std::max(max_len, int(n.first.size()) + 1);
    for (auto p : pos_opts)
        max_len = std::max(max_len, int(std::get<0>(p).size()) + 2);

    out << "Options: " << std::endl;
    write_rpad(out, "-help", max_len + 4);
    out << "show this help" << std::endl;
    for (auto n : named_opts) {
        write_rpad(out, "-" + n.first, max_len + 4);
        out << n.second.second << std::endl;
    }
    out << "Positionals: " << std::endl;
    for (auto &[name, optional, help] : pos_opts) {
        std::string field = "";
        field += (optional ? '[' : '<');
        field += name;
        field += (optional ? ']' : '>');
        write_rpad(out, field, max_len + 4);
        out << help << std::endl;
    }
    out << std::endl;
}

bool CmdlineParser::parse(int argc, const char *argv[], int arg_offset, std::ostream &out, CmdlineResult &result) {
    int cursor = arg_offset;
    result.positional.clear();
    result.named.clear();
    while (cursor < argc) {
        const char *arg = argv[cursor];
        if (arg[0] == '\0' || arg[0] != '-')
            break;
        ++cursor;
        const std::string opt(arg + 1);
        if (opt == "help") {
            print_help(argv, arg_offset, out);
            return false;
        }
        auto found = named_opts.find(opt);
        if (found == named_opts.end()) {
            out << "No option named '" << arg << "'" << std::endl;
            print_help(argv, arg_offset, out);
            return false;
        }
        auto &o = result.named[opt];
        for (int i = 0; i < found->second.first; i++) {
            if (cursor >= argc) {
                out << "Not enough arguments given to '" << arg << "'" << std::endl;
                print_help(argv, arg_offset, out);
                return false;
            }
            o.emplace_back(argv[cursor++]);
        }
    }
    for (auto &[name, optional, help] : pos_opts) {
        (void) help;
        if (cursor >= argc) {
            if (!optional) {
                out << "Not enough positional arguments" << std::endl;
                print_help(argv, arg_offset, out);
                return false;
            } else {
                break;
            }
        }
        result.positional.emplace_back(argv[cursor++]);
    }
    return true;
}

MEOW_NAMESPACE_END
