#pragma once
// nnh's subcommands (spec 010 FR-2..FR-6). Each returns the process exit status.

#include <functional>
#include <string>
#include <vector>

#include "cli.hpp"
#include "manifest.hpp"
#include "repo.hpp"

namespace nnh::tool {

int command_build(const Options& options);
int command_run(const Options& options);
int command_install(const Options& options);

/// What building a game produced (commands/build.cpp).
struct BuiltGame {
    std::string root;   ///< the project (a game project or the engine repository)
    std::string preset;
    GameEntry game;     ///< binary_dir as a native path
};

/// Receives build output lines; returning false stops the build (nnh serve: the console left).
using OutputFn = std::function<bool(const std::string&)>;

/// Configures the preset if needed (or if the game isn't in its manifest yet) and builds `<game><suffix>`
/// (e.g. "hello_3dsx"); with `need_cia`, refuses a game without a .cia before building. Prints why on failure.
/// With `output`, every message and output line also goes there.
bool build_target(const Options& options, const std::string& target_suffix, bool release, BuiltGame& out,
                  bool need_cia = false, const OutputFn* output = nullptr);
/// build_target in a project already opened (nnh serve: nnh.toml read once, when it started).
bool build_target_in(const std::string& root, const Options& options, const std::string& target_suffix, bool release,
                     BuiltGame& out, bool need_cia = false, const OutputFn* output = nullptr);

/// The project nnh works in (commands/project.cpp, spec 032 FR-11, FR-12): the nearest game project (nnh.toml) or
/// engine repository at or above the current folder, its nnh.toml read (warnings printed). "" after printing why: no
/// project, or a malformed nnh.toml.
std::string open_project(const OutputFn* output = nullptr);
/// open_project from the folder `start` instead of the current one.
std::string open_project_at(const std::string& start, const OutputFn* output = nullptr);

/// Whether `root`'s CMakePresets.json (or CMakeUserPresets.json) has a configure and a build preset `preset`; prints
/// which one is missing otherwise (spec 032 FR-13), before CMake runs.
bool check_preset(const std::string& root, const std::string& preset, const OutputFn* output = nullptr);

/// The configure command of `preset` in `root` (repo.hpp configure_command): on Windows 3DS presets with the host
/// compiler and, in a game project, NNHGE_ENGINE_DIR (spec 032 FR-14, FR-15). "" when a path can't be passed.
std::string project_configure_command(const std::string& root, const std::string& preset, const Shell& shell);

/// Checks the preset (check_preset), configures it in `root` if it never was, then reads its games manifest.
bool configure_and_list(const std::string& root, const std::string& preset, bool for_3ds, std::vector<GameEntry>& games,
                        const OutputFn* output = nullptr);

struct GameRun;
/// nnh run --pc (commands/run_pc.cpp, spec 013): builds (unless --no-build), starts the simulator with
/// NNH_REMOTE pointing back at nnh and follows it with `run`. @return the script's exit code, or the
/// simulator's without a script.
int run_on_pc(const Options& options, const GameRun& run);

/// Hot reload (spec 031, commands/run_reload.cpp): once `built` is built and connected, sets up `run.reload` (reads
/// <binary dir>/nnh_reload_<game>.txt, scans the outputs) and gives it to the runner; notes why when it can't.
void start_reload(const GameRun& run, const BuiltGame& built, bool pc);

/// nnh restore-assets <game> (spec 031): puts back the sources an interrupted script left swapped.
int command_restore_assets(const Options& options);
/// The end of a run (spec 031): every swapped source put back, the asset target rebuilt so the outputs match again.
void finish_reload(const GameRun& run);

int command_serve(const Options& options);

/// nnh new <name> (commands/new.cpp, spec 033 FR-3): writes a game project into ./<name>. 0 created, 1 refused or failed.
int command_new(const Options& options);
int command_unpair();

} // namespace nnh::tool
