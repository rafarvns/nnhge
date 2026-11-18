#pragma once
// nnh's subcommands (spec 010 FR-2..FR-6). Each returns the process exit status.

#include <string>

#include "cli.hpp"
#include "manifest.hpp"

namespace nnh::tool {

int command_build(const Options& options);
int command_run(const Options& options);
int command_install(const Options& options);

/// What building a game produced (commands/build.cpp).
struct BuiltGame {
    std::string root;   ///< the repository
    std::string preset;
    GameEntry game;     ///< binary_dir as a native path
};

/// Configures the preset if needed (or if the game isn't in its manifest yet) and builds `<game><suffix>`
/// (e.g. "hello_3dsx"); with `need_cia`, refuses a game without a .cia before building. Prints why on failure.
bool build_target(const Options& options, const std::string& target_suffix, bool release, BuiltGame& out,
                  bool need_cia = false);

} // namespace nnh::tool
