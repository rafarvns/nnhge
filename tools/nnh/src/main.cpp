// nnh — the NNHGE command-line tool: build, run on the console with readable logs, install (spec 010); serve
// NNH Link (spec 011); drive a Development game with test scripts or typed commands (spec 013).

#include <cstdio>
#include <string>
#include <vector>

#include "cli.hpp"
#include "commands.hpp"
#include "link_types.hpp"

int main(int argc, char** argv) {
    using namespace nnh::tool;
    std::vector<std::string> args(argv + 1, argv + argc);
    Options options;
    std::string error;
    if (!parse_command_line(args, options, error)) {
        std::fprintf(stderr, "nnh: %s\n", error.c_str());
        return 2;
    }
    if (!net::startup()) {
        std::fprintf(stderr, "nnh: the network library didn't start\n");
        return 1;
    }
    switch (options.command) {
    case Command::Build: return command_build(options);
    case Command::Run: return command_run(options);
    case Command::Install: return command_install(options);
    case Command::Serve: return command_serve(options);
    case Command::Unpair: return command_unpair();
    case Command::Help: break;
    }
    std::fputs(help_text(), stdout);
    return 0;
}
