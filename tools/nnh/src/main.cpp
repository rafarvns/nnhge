// nnh — the NNHGE command-line tool (spec 010). Stage 1: the command line; the commands come in stage 3.

#include <cstdio>
#include <string>
#include <vector>

#include "cli.hpp"

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    nnh::tool::Options options;
    std::string error;
    if (!nnh::tool::parse_command_line(args, options, error)) {
        std::fprintf(stderr, "nnh: %s\n", error.c_str());
        return 2;
    }
    if (options.command == nnh::tool::Command::Help) {
        std::fputs(nnh::tool::help_text(), stdout);
        return 0;
    }
    std::fprintf(stderr, "nnh: this command isn't implemented yet (spec 010 stage 3)\n");
    return 1;
}
