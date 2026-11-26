#pragma once
// Reconverting assets (spec 031 FR-2, FR-3, FR-14, plan question 4): the game's asset target is built exactly as a
// normal build would (`cmake --build --preset P --target <game>_assets`, through devkitPro's MSYS bash for the 3DS on
// Windows); its output is shown as a build shows it (`file:line: reason`); a failure is retried once after 200 ms (a file
// the simulator held open for a moment). Then a Ninja dry run of the game's program (`--target <game> -- -n`): any command
// left means the change needs the program rebuilt (a schema source, IMAGES, a CMake declaration) — refused, "rebuild and
// restart". Tests: tools/nnh/tests/test_asset_build.cpp.

#include <functional>
#include <string>
#include <vector>

#include "repo.hpp"

namespace nnh::tool::reload {

struct BuildCommands {
    std::string assets;  ///< builds <game>_assets
    std::string dry_run; ///< lists what building <game> would still do
};

BuildCommands build_commands(const std::string& root, const std::string& preset, const std::string& game,
                             const Shell& shell);

/// Runs a shell command, collecting its output lines. @return its exit status.
using CommandRunner = std::function<int(const std::string& command, std::vector<std::string>& lines)>;
/// The real runner (run_shell_lines).
CommandRunner shell_runner(const Shell& shell);

struct BuildResult {
    bool ok = false;
    std::vector<std::string> lines; ///< the build's output (shown as is)
    int attempts = 0;
};

/// Builds the asset target (one retry after `retry_ms` on failure).
BuildResult build_assets(const BuildCommands& commands, const CommandRunner& run, int retry_ms = 200);

/// The first command Ninja's dry run lists ("Building CXX object examples/data/.../data_types.cpp.obj"), or "" when
/// nothing is left ("ninja: no work to do.", or no "[n/m]" line).
std::string first_dry_run_command(const std::vector<std::string>& lines);

} // namespace nnh::tool::reload
