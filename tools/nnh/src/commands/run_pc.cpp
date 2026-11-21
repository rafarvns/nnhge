// nnh run --pc (spec 010 FR-3; spec 013 FR-4): the simulator instead of the console. nnh listens on a free
// loopback port and starts the simulator with NNH_REMOTE=127.0.0.1:<port>, so a Debug/Development build
// connects back: its log then comes through nnh (shown and saved like a console's) and scripts or typed
// commands drive it exactly as on the console. A Release build doesn't connect and just runs.

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <thread>

#include "commands.hpp"
#include "game_log.hpp"
#include "link_types.hpp"
#include "remote/script_runner.hpp"
#include "repo.hpp"

namespace nnh::tool {

namespace {

void set_remote_env(const std::string& value) {
#ifdef _WIN32
    _putenv_s("NNH_REMOTE", value.c_str());
#else
    setenv("NNH_REMOTE", value.c_str(), 1);
#endif
}

} // namespace

int run_on_pc(const Options& options, const GameRun& run) {
    const bool scripted = run.runner != nullptr && !run.runner->interactive();
    const int cannot_run = scripted ? 2 : 1;
    BuiltGame built;
    if (!build_target(options, "", options.release, built)) return cannot_run;
#ifdef _WIN32
    const std::string exe = built.game.binary_dir + "/" + options.game + ".exe";
#else
    const std::string exe = built.game.binary_dir + "/" + options.game;
#endif
    std::error_code ec;
    if (!std::filesystem::exists(exe, ec)) {
        std::fprintf(stderr, "nnh: %s isn't built (run without --no-build)\n", exe.c_str());
        return cannot_run;
    }
    net::Socket listener = net::tcp_listen(Ipv4{{127, 0, 0, 1}}, 0);
    if (!listener.valid()) {
        std::fprintf(stderr, "nnh: can't listen on the loopback interface for the simulator\n");
        return cannot_run;
    }
    set_remote_env("127.0.0.1:" + std::to_string(listener.local_port())); // inherited by the simulator
    std::atomic<bool> ended{false};
    int status = 0;
    std::thread simulator([&] {
        status = run_shell("\"" + exe + "\"", host_shell(false));
        ended = true;
    });
    net::Socket game;
    while (!ended && !game.valid()) game = net::tcp_accept(listener, 200);
    listener.close();
    const bool connected = game.valid();
    int code = 2;
    if (connected) {
        GameRun pc_run = run;
        pc_run.linger_ms = 10000; // Bye{quit} closes the simulator: show its last lines
        code = follow_game(game, built.root, options.game, pc_run);
        game.close();
    } else if (scripted) {
        std::fprintf(stderr, "nnh: the simulator closed without connecting to nnh (a Release build?)\n");
    }
    simulator.join();
    return scripted ? code : status;
}

} // namespace nnh::tool
