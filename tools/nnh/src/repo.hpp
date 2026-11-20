#pragma once
// The repository nnh works in (spec 010 FR-2): its root, the presets, running CMake (through devkitPro's
// MSYS2 bash for the 3DS on Windows, like scripts/dkp.sh), and paths. The command and path helpers are
// pure and unit tested in tools/nnh/tests/test_repo.cpp.

#include <ctime>
#include <functional>
#include <string>

namespace nnh::tool {

/// Walks up from `start` to the folder holding CMakePresets.json and engine/core/. Empty if none.
std::string find_repo_root(const std::string& start);

/// "3ds-dev", "3ds-release", "pc-dev" or "pc-release".
std::string preset_name(bool pc, bool release);

/// "/e/x/y" (MSYS, written by devkitPro's CMake on Windows) -> "E:/x/y"; other paths unchanged.
std::string native_path(const std::string& path);
/// "E:/x/y" or "E:\x\y" -> "/e/x/y".
std::string msys_path(const std::string& path);

struct Shell {
    bool windows = false;      ///< build commands for cmd.exe
    std::string devkitpro_bash; ///< non-empty: run CMake through this MSYS2 bash (3DS on Windows)
};

/// The shell command that runs `cmake <args>` in `root`.
std::string cmake_command(const std::string& root, const std::string& args, const Shell& shell);

/// The Shell for this computer and target (3DS presets go through devkitPro's bash on Windows;
/// DEVKITPRO_MSYS2 overrides its location, default C:/devkitPro/msys2).
Shell host_shell(bool for_3ds);

/// Runs a shell command, showing its output. @return its exit status.
int run_shell(const std::string& command, const Shell& shell);

/// Runs a shell command and gives each line of its output (stdout and stderr, '\r' removed) to `on_line`.
/// When `on_line` returns false, nnh stops reading: the command fails on its next write and is waited for.
/// @return its exit status, or -1 when stopped.
int run_shell_lines(const std::string& command, const Shell& shell,
                    const std::function<bool(const std::string&)>& on_line);

/// "<root>/build/logs/<game>-YYYYMMDD-HHMMSS.txt" for local time `t`.
std::string log_file_path(const std::string& root, const std::string& game, const std::tm& t);

} // namespace nnh::tool
