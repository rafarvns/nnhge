// nnh build (spec 010 FR-2): configure the preset once, build the game's target, find it in the manifest.
// nnh serve (spec 011) uses the same steps with the output captured line by line.

#include <cstdio>
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
        if (report.run(cmake_command(root, "--preset " + preset, shell), shell) != 0) {
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
    std::error_code ec;
    out.root = find_repo_root(std::filesystem::current_path(ec).generic_string());
    if (out.root.empty()) {
        report.say("nnh: run it inside the NNHGE repository (no CMakePresets.json above here)", true);
        return false;
    }
    out.preset = preset_name(options.pc, release);
    const Shell shell = host_shell(!options.pc);
    std::vector<GameEntry> games;
    if (!configure_and_list(out.root, out.preset, !options.pc, games, output)) return false;
    auto game = find_game(games, options.game);
    if (!game) { // a new game, or a build folder configured before the manifest existed: configure again
        if (report.run(cmake_command(out.root, "--preset " + out.preset, shell), shell) != 0) return false;
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
