#pragma once
// The project nnh works in (spec 010 FR-2; a game project or the engine repository since spec 032, project_root.hpp):
// the presets, running CMake (through devkitPro's MSYS2 bash for the 3DS on Windows, like scripts/dkp.sh), and paths.
// The command and path helpers are pure and unit tested in tools/nnh/tests/test_repo.cpp and
// test_configure_command.cpp.

#include <ctime>
#include <functional>
#include <string>

namespace nnh::tool {

/// "3ds-dev", "3ds-release", "pc-dev" or "pc-release".
std::string preset_name(bool pc, bool release);

/// The preset to use: `override` (NNH_PRESET) when it names a preset of the same system ("pc-…" for the PC,
/// "3ds-…" for the 3DS; letters, digits and '-' only), else preset_name(pc, release).
std::string preset_for(bool pc, bool release, const char* override);

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

/// What a configure hands to devkitPro's MSYS2 CMake besides the preset (spec 032 FR-14, FR-15, D55 items 7 and 8), in
/// the form the caller found them (drive paths on Windows); configure_command converts them.
struct ConfigureExtras {
    std::string host_cxx;   ///< the host C++ compiler (host_compiler.hpp), "" = none found
    std::string engine_dir; ///< NNHGE_ENGINE_DIR, the local engine of a game project, "" = none
};

/// The shell command that configures `preset` in `root`. Through devkitPro's bash (a 3DS preset on Windows) it adds
/// `-DNNH_HOST_CXX=<msys path>` and `-DFETCHCONTENT_SOURCE_DIR_NNHGE=<msys path>` for the extras given, each
/// single-quoted (spaces allowed), because that CMake reads "E:/..." as a relative path and the shell has no host
/// compiler. Elsewhere `cmake --preset <preset>` unchanged (PC presets map NNHGE_ENGINE_DIR themselves; Linux CMake
/// finds the host compiler). "" when a path holds ' " $ ` or %, which the shell (bash -lc "...", cmd.exe) would read itself.
std::string configure_command(const std::string& root, const std::string& preset, const Shell& shell,
                              const ConfigureExtras& extras);

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

/// The folder for a run's screenshots (spec 013): the log file's path without ".txt", next to it.
std::string screenshot_dir(const std::string& log_path);

} // namespace nnh::tool
