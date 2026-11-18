#pragma once
// nnh's command line (spec 010 FR-1..FR-6): `nnh <command> [<game>] [options]`. Pure; unit tested in
// tools/nnh/tests/test_cli.cpp.

#include <string>
#include <vector>

namespace nnh::tool {

enum class Command { Help, Build, Run, Install };

struct Options {
    Command command = Command::Help;
    std::string game;
    bool release = false; ///< --release: the Release preset (3DS)
    bool pc = false;      ///< --pc: the simulator instead of the console
    std::string console;  ///< --console <ip>
};

/// Parses argv[1..]. @return false with `error` set for an unknown command/option or a missing value.
bool parse_command_line(const std::vector<std::string>& args, Options& out, std::string& error);

/// The text of `nnh help`.
const char* help_text();

} // namespace nnh::tool
