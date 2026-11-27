// The project a command works in (spec 032 FR-11 to FR-15): found, its nnh.toml read, its presets checked, its
// configure command built with what devkitPro's MSYS2 CMake needs.

#include <cstdio>
#include <cstdlib>
#include <filesystem>

#include "commands.hpp"
#include "host_compiler.hpp"
#include "preset_names.hpp"
#include "project_file.hpp"
#include "project_root.hpp"
#include "repo.hpp"

namespace nnh::tool {

namespace {

std::string read_bounded(const std::string& path, std::size_t limit) {
    std::string text;
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (f == nullptr) return text;
    char buffer[4096];
    std::size_t n = 0;
    while ((n = std::fread(buffer, 1, sizeof buffer, f)) > 0 && text.size() <= limit) text.append(buffer, n);
    std::fclose(f);
    return text;
}

void say(const OutputFn* output, const std::string& text, bool error) {
    std::fprintf(error ? stderr : stdout, "%s\n", text.c_str());
    if (output != nullptr) (*output)(text);
}

} // namespace

std::string open_project(const OutputFn* output) {
    std::error_code ec;
    const ProjectRoot root = find_project_root(std::filesystem::current_path(ec).generic_string());
    if (root.path.empty()) {
        say(output, no_project_message(), true);
        return {};
    }
    if (!root.game) return root.path;
    const ProjectFile file = read_project_file(read_bounded(root.path + "/nnh.toml", kMaxProjectFileBytes));
    for (const std::string& w : file.warnings) say(output, "nnh: " + w, false);
    for (const std::string& e : file.errors) say(output, "nnh: " + e, true);
    return file.errors.empty() ? root.path : std::string();
}

bool check_preset(const std::string& root, const std::string& preset, const OutputFn* output) {
    if (presets_file_defines(root + "/CMakePresets.json", root + "/CMakeUserPresets.json", preset)) return true;
    say(output, "nnh: this project has no preset \"" + preset +
                    "\" (CMakePresets.json): add the six presets (docs/guides/game-projects.md)", true);
    return false;
}

std::string project_configure_command(const std::string& root, const std::string& preset, const Shell& shell) {
    ConfigureExtras extras;
    if (!shell.devkitpro_bash.empty()) { // a 3DS preset on Windows: devkitPro's shell has no host compiler
        extras.host_cxx = host_cxx();
        std::error_code ec;
        const char* engine = std::getenv("NNHGE_ENGINE_DIR");
        if (engine != nullptr && std::filesystem::is_regular_file(root + "/nnh.toml", ec)) extras.engine_dir = engine;
    }
    return configure_command(root, preset, shell, extras);
}

} // namespace nnh::tool
