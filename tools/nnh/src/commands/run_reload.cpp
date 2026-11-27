// nnh run's hot reload (spec 031 FR-1, FR-2, FR-14, FR-22): once the game is built and connected, the controller of this
// run — the build of `<game>_assets` and the dry run of `<game>` through the preset's shell — is given to the runner;
// interactive runs watch the sources, scripts swap them (`swap-asset`). At the end every swapped source is put back and
// the asset target rebuilt, so the converted outputs match the sources again. `nnh restore-assets <game>` does the same
// after an interrupted script.

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

#include "commands.hpp"
#include "game_log.hpp"
#include "reload/asset_build.hpp"
#include "reload/asset_swap.hpp"
#include "reload/interrupt.hpp"
#include "reload/reload_setup.hpp"
#include "remote/script_runner.hpp"
#include "repo.hpp"

namespace nnh::tool {
namespace {


reload::ControllerSetup controller_setup(const reload::ReloadPaths& paths, const BuiltGame& built, bool pc) {
    const Shell shell = host_shell(!pc);
    const std::string ninja = reload::cached_ninja(built.root + "/build/" + built.preset); // PC, or devkitPro's (MSYS)
    const reload::BuildCommands commands =
        reload::build_commands(built.root, built.preset, built.game.name, shell, ninja);
    const reload::CommandRunner runner = reload::shell_runner(shell);
    reload::ControllerSetup setup;
    setup.out_dir = paths.out;
    setup.src_dir = paths.assets;
    setup.game_dir = paths.game;
    const std::string build_dir = built.root + "/build/" + built.preset;
    setup.build = [commands, runner, build_dir](const std::string& outside_source) {
        const auto start = std::chrono::steady_clock::now();
        reload::BuildOutcome out;
        const reload::BuildResult built_assets = reload::build_assets(commands, runner);
        out.ok = built_assets.ok;
        out.lines = built_assets.lines;
        if (out.ok && !outside_source.empty() && !commands.program_inputs.empty()) {
            std::vector<std::string> inputs;
            runner(commands.program_inputs, inputs);
            const std::string input = reload::program_input(inputs, outside_source, build_dir);
            if (!input.empty()) out.restart = input + " is compiled into the program";
        }
        out.ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
        return out;
    };
    setup.now_ms = [] {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
            .count();
    };
    return setup;
}

} // namespace

void start_reload(const GameRun& run, const BuiltGame& built, bool pc) {
    if (run.reload == nullptr || run.runner == nullptr) return;
    const auto paths = reload::read_reload_paths(built.game.binary_dir, built.game.name);
    if (!paths) {
        std::printf("nnh: hot reload off: %s has no assets folder (or was configured before spec 031)\n",
                    built.game.name.c_str());
        return;
    }
    reload::ReloadSession& s = *run.reload;
    s.controller = std::make_unique<reload::ReloadController>(controller_setup(*paths, built, pc));
    s.controller->start();
    s.controller->set_watching(s.watch);
    run.runner->set_reload(s.controller.get());
    if (!s.watch) { // a script: swaps
        s.backup_dir = reload::swap_backup_dir(built.root, built.game.name);
        s.swaps = std::make_unique<reload::AssetSwaps>(paths->game, s.backup_dir);
        run.runner->set_reload_script(s.swaps.get(), s.script_dir);
        reload::install_interrupt(); // Ctrl+C ends the script; finish_reload() puts the sources back (review S6)
        return;
    }
    std::printf("nnh: hot reload on: saving a file in %s reloads it (type reload to force one)\n",
                paths->assets.c_str());
}

void finish_reload(const GameRun& run) {
    if (run.reload == nullptr || run.reload->swaps == nullptr) return;
    reload::ReloadSession& s = *run.reload;
    const int restored = s.swaps->restore_all();
    reload::interrupt_handled();
    reload::uninstall_interrupt();
    if (restored == 0) return;
    std::printf("nnh: put back %d swapped asset%s\n", restored, restored == 1 ? "" : "s");
    if (s.controller != nullptr) s.controller->rebuild(); // the outputs match the sources again
}

int command_restore_assets(const Options& options) {
    const std::string root = find_repo_root(std::filesystem::current_path().string());
    if (root.empty()) {
        std::fprintf(stderr, "nnh: not inside an NNHGE repository\n");
        return 1;
    }
    const std::string backup = reload::swap_backup_dir(root, options.game);
    const std::string game_dir = reload::journal_game_dir(backup);
    if (game_dir.empty()) {
        std::printf("nnh: nothing to restore for %s\n", options.game.c_str());
        return 0;
    }
    reload::AssetSwaps swaps(game_dir, backup);
    const int n = swaps.restore_all();
    std::printf("nnh: put back %d asset%s of %s (rebuild with nnh run or nnh build)\n", n, n == 1 ? "" : "s",
                options.game.c_str());
    return 0;
}

} // namespace nnh::tool
