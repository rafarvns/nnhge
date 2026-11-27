// nnh build (spec 010 FR-2): configure the preset once, build the game's target, find it in the manifest.
// nnh serve (spec 011) uses the same steps with the output captured line by line. Since spec 032 the project is a game
// project or the engine repository, its preset checked before CMake runs (commands/project.cpp).

#include <cstdio>
#include <cstdlib>
#include <filesystem>

#include "commands.hpp"
#include "repo.hpp"

namespace nnh::tool {

namespace {

std::string read_file(const std::string& path) {
    std::string text;
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (f == nullptr) return text;
    char buffer[4096];
    std::size_t n = 0;
    while ((n = std::fread(buffer, 1, sizeof buffer, f)) > 0) text.append(buffer, n);
    std::fclose(f);
    return text;
}

/// Where build_target's messages and command output go: the terminal, and `output` when given.
struct Reporter {
    const OutputFn* output;
    bool alive = true; ///< false once `output` asked to stop

    void say(const std::string& text, bool error) {
        std::fprintf(error ? stderr : stdout, "%s\n", text.c_str());
        if (output != nullptr && alive) alive = (*output)(text);
    }
    int run(const std::string& command, const Shell& shell) {
        if (output == nullptr) return run_shell(command, shell);
        return run_shell_lines(command, shell, [this](const std::string& line) {
            std::printf("%s\n", line.c_str());
            alive = alive && (*output)(line);
            return alive;
        });
    }
};

} // namespace

bool configure_and_list(const std::string& root, const std::string& preset, bool for_3ds, std::vector<GameEntry>& games,
                        const OutputFn* output) {
    Reporter report{output};
    const Shell shell = host_shell(for_3ds);
    const std::string build_dir = root + "/build/" + preset;
    std::error_code ec;
    if (!std::filesystem::exists(build_dir + "/CMakeCache.txt", ec)) {
        report.say("nnh: configuring " + preset, false);
        const std::string command = project_configure_command(root, preset, shell);
        if (command.empty()) {
            report.say("nnh: a path holds a quote (' or \"): devkitPro's shell can't be given it", true);
            return false;
        }
        if (report.run(command, shell) != 0) {
            report.say("nnh: configuring " + preset + " failed", true);
            return false;
        }
    }
    games = parse_manifest(read_file(build_dir + "/nnh_games.txt"));
    return true;
}

bool build_target(const Options& options, const std::string& target_suffix, bool release, BuiltGame& out,
                  bool need_cia, const OutputFn* output) {
    Reporter report{output};
    out.root = open_project(output); // a game project (nnh.toml) or the engine repository (spec 032)
    if (out.root.empty()) return false;
    out.preset = preset_for(options.pc, release, std::getenv("NNH_PRESET"));
    if (!check_preset(out.root, out.preset, output)) return false;
    const Shell shell = host_shell(!options.pc);
    std::vector<GameEntry> games;
    if (!configure_and_list(out.root, out.preset, !options.pc, games, output)) return false;
    auto game = find_game(games, options.game);
    if (!game) { // a new game, or a build folder configured before the manifest existed: configure again
        const std::string command = project_configure_command(out.root, out.preset, shell);
        if (command.empty() || report.run(command, shell) != 0) return false;
        games = parse_manifest(read_file(out.root + "/build/" + out.preset + "/nnh_games.txt"));
        game = find_game(games, options.game);
    }
    if (!game) {
        std::string names;
        for (const GameEntry& g : games) names += " " + g.name;
        report.say("nnh: no game '" + options.game + "' in " + out.preset + " (games:" + names + ")", true);
        return false;
    }
    out.game = *game;
    out.game.binary_dir = native_path(out.game.binary_dir);
    if (need_cia && !out.game.cia) {
        report.say("nnh: " + options.game +
                       " has no .cia: add CIA UNIQUE_ID ... to its nnh_add_game() (docs/guides/packaging.md)", true);
        return false;
    }
    const std::string target = options.game + target_suffix;
    if (options.no_build) {
        report.say("nnh: not building " + target + " (--no-build): running what " + out.preset + " has", false);
        return report.alive;
    }
    report.say("nnh: building " + target + " (" + out.preset + ")", false);
    if (report.run(cmake_command(out.root, "--build --preset " + out.preset + " --target " + target, shell), shell) != 0) {
        report.say("nnh: the build failed", true);
        return false;
    }
    return report.alive;
}

int command_build(const Options& options) {
    BuiltGame built;
    return build_target(options, options.pc ? "" : "_3dsx", options.release, built) ? 0 : 1;
}

} // namespace nnh::tool
