// nnh run's hot reload (spec 031 FR-1, FR-2, FR-14): once the game is built and connected, the controller of this run —
// the build of `<game>_assets` and the dry run of `<game>` through the preset's shell — is given to the runner.

#include <chrono>
#include <cstdio>

#include "commands.hpp"
#include "game_log.hpp"
#include "reload/asset_build.hpp"
#include "reload/reload_setup.hpp"
#include "remote/script_runner.hpp"
#include "repo.hpp"

namespace nnh::tool {

void start_reload(const GameRun& run, const BuiltGame& built, bool pc) {
    if (run.reload == nullptr || run.runner == nullptr) return;
    const auto paths = reload::read_reload_paths(built.game.binary_dir, built.game.name);
    if (!paths) {
        std::printf("nnh: hot reload off: %s has no assets folder (or was configured before spec 031)\n",
                    built.game.name.c_str());
        return;
    }
    const Shell shell = host_shell(!pc);
    const reload::BuildCommands commands = reload::build_commands(built.root, built.preset, built.game.name, shell);
    const reload::CommandRunner runner = reload::shell_runner(shell);
    reload::ControllerSetup setup;
    setup.out_dir = paths->out;
    setup.src_dir = paths->assets;
    setup.build = [commands, runner] {
        const auto start = std::chrono::steady_clock::now();
        reload::BuildOutcome out;
        const reload::BuildResult built_assets = reload::build_assets(commands, runner);
        out.ok = built_assets.ok;
        out.lines = built_assets.lines;
        if (out.ok) {
            std::vector<std::string> dry;
            runner(commands.dry_run, dry);
            out.restart = reload::first_dry_run_command(dry);
        }
        out.ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
        return out;
    };
    setup.now_ms = [] {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
            .count();
    };
    run.reload->controller = std::make_unique<reload::ReloadController>(setup);
    run.reload->controller->start();
    run.reload->controller->set_watching(true);
    run.runner->set_reload(run.reload->controller.get());
    std::printf("nnh: hot reload on: saving a file in %s reloads it (type reload to force one)\n",
                paths->assets.c_str());
}

} // namespace nnh::tool
