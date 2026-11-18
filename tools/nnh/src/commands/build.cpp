// nnh build (spec 010 FR-2): configure the preset once, build the game's target, find it in the manifest.

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

} // namespace

bool build_target(const Options& options, const std::string& target_suffix, bool release, BuiltGame& out,
                  bool need_cia) {
    std::error_code ec;
    out.root = find_repo_root(std::filesystem::current_path(ec).generic_string());
    if (out.root.empty()) {
        std::fprintf(stderr, "nnh: run it inside the NNHGE repository (no CMakePresets.json above here)\n");
        return false;
    }
    out.preset = preset_name(options.pc, release);
    const Shell shell = host_shell(!options.pc);
    const std::string build_dir = out.root + "/build/" + out.preset;
    if (!std::filesystem::exists(build_dir + "/CMakeCache.txt", ec)) {
        std::printf("nnh: configuring %s\n", out.preset.c_str());
        if (run_shell(cmake_command(out.root, "--preset " + out.preset, shell), shell) != 0) {
            std::fprintf(stderr, "nnh: configuring %s failed\n", out.preset.c_str());
            return false;
        }
    }
    auto games = parse_manifest(read_file(build_dir + "/nnh_games.txt"));
    auto game = find_game(games, options.game);
    if (!game) { // a new game, or a build folder configured before the manifest existed: configure again
        if (run_shell(cmake_command(out.root, "--preset " + out.preset, shell), shell) != 0) return false;
        games = parse_manifest(read_file(build_dir + "/nnh_games.txt"));
        game = find_game(games, options.game);
    }
    if (!game) {
        std::fprintf(stderr, "nnh: no game '%s' in %s (games:", options.game.c_str(), out.preset.c_str());
        for (const GameEntry& g : games) std::fprintf(stderr, " %s", g.name.c_str());
        std::fprintf(stderr, ")\n");
        return false;
    }
    out.game = *game;
    out.game.binary_dir = native_path(out.game.binary_dir);
    if (need_cia && !out.game.cia) {
        std::fprintf(stderr, "nnh: %s has no .cia: add CIA UNIQUE_ID ... to its nnh_add_game() (docs/guides/packaging.md)\n",
                     options.game.c_str());
        return false;
    }
    const std::string target = options.game + target_suffix;
    std::printf("nnh: building %s (%s)\n", target.c_str(), out.preset.c_str());
    if (run_shell(cmake_command(out.root, "--build --preset " + out.preset + " --target " + target, shell), shell) != 0) {
        std::fprintf(stderr, "nnh: the build failed\n");
        return false;
    }
    return true;
}

int command_build(const Options& options) {
    BuiltGame built;
    return build_target(options, options.pc ? "" : "_3dsx", options.release, built) ? 0 : 1;
}

} // namespace nnh::tool
