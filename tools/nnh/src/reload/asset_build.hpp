#pragma once
// Reconverting assets (spec 031 FR-2, FR-3, FR-14, plan question 4): the game's asset target is built exactly as a
// normal build would (the preset's Ninja on `<game>_assets`, else `cmake --build --preset P`; inside devkitPro's MSYS
// bash for the 3DS on Windows); its output is shown as a build shows it (`file:line: reason`); a failure is retried
// once after 200 ms (a file the simulator held open for a moment). A changed source outside the assets folder (a script's swap of a schema source)
// is looked up among the inputs of the game's program (`ninja -t inputs <game>`): one of them means the program must be
// rebuilt — refused, "rebuild and restart". (A dry run `ninja -n <game>` can't tell: CMake's glob check makes it stop at
// "Re-running CMake" in every game with an assets folder.) Tests: tools/nnh/tests/test_reload_tool.cpp.

#include <functional>
#include <string>
#include <vector>

#include "repo.hpp"

namespace nnh::tool::reload {

struct BuildCommands {
    std::string assets;  ///< builds <game>_assets
    std::string program_inputs; ///< lists every input of the program <game> ("" without a known Ninja: no check)
};

/// With `ninja` (the preset's CMAKE_MAKE_PROGRAM) Ninja is called in the preset's build folder, directly or (devkitPro's
/// MSYS shell, `ninja` an MSYS path) inside that shell; without it, `cmake --build --preset`.
BuildCommands build_commands(const std::string& root, const std::string& preset, const std::string& game,
                             const Shell& shell, const std::string& ninja = "");
/// CMAKE_MAKE_PROGRAM from <build_dir>/CMakeCache.txt when it is Ninja, else "".
std::string cached_ninja(const std::string& build_dir);

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

/// The line of `ninja -t inputs` naming `source` (an absolute path; the lines may be absolute, MSYS `/e/...` or relative
/// to `build_dir`; compared without case and with either slash), or "".
std::string program_input(const std::vector<std::string>& inputs, const std::string& source,
                          const std::string& build_dir);

} // namespace nnh::tool::reload
