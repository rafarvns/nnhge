#pragma once
// nnh's command line (spec 010 FR-1..FR-6; serve and unpair: spec 011): `nnh <command> [<game>] [options]`. Pure; unit tested in
// tools/nnh/tests/test_cli.cpp.

#include <string>
#include <vector>

namespace nnh::tool {

enum class Command { Help, Build, Run, Install, Serve, Unpair };

struct Options {
    Command command = Command::Help;
    std::string game;
    bool release = false; ///< --release: the Release preset (3DS)
    bool pc = false;      ///< --pc: the simulator instead of the console
    std::string console;  ///< --console <ip>
    std::string script;   ///< run --script <file.nnhs>: a test script (spec 013); empty = none
    bool no_build = false; ///< run --no-build: run what is already built
};

/// Parses argv[1..]. @return false with `error` set for an unknown command/option or a missing value.
bool parse_command_line(const std::vector<std::string>& args, Options& out, std::string& error);

/// nnh run (spec 013): whether the Bye at the end of a session closes the game — only a script's, and only in
/// the simulator (--pc). On the console the game keeps running; a typed `end` only detaches.
bool closes_game_at_end(const Options& options);

/// The text of `nnh help`.
const char* help_text();

} // namespace nnh::tool
