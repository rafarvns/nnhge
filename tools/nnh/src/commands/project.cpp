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
#include "text_file.hpp"

namespace nnh::tool {

namespace {

void say(const OutputFn* output, const std::string& text, bool error) {
    std::fprintf(error ? stderr : stdout, "%s\n", text.c_str());
    if (output != nullptr) (*output)(text);
}

} // namespace

std::string open_project(const OutputFn* output) {
    std::error_code ec;
    return open_project_at(std::filesystem::current_path(ec).generic_string(), output);
}

std::string open_project_at(const std::string& start, const OutputFn* output) {
    const ProjectRoot root = find_project_root(start);
    if (root.path.empty()) {
        say(output, no_project_message(), true);
        return {};
    }
    if (!root.game) return root.path;
    const ProjectFile file = read_project_file(read_text_file(root.path + "/nnh.toml", kMaxProjectFileBytes));
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
    std::error_code ec;
    const bool game = std::filesystem::is_regular_file(root + "/nnh.toml", ec);
    return configure_command(root, preset, shell,
                             configure_extras(game, !shell.devkitpro_bash.empty(), std::getenv("NNHGE_ENGINE_DIR"),
                                              std::getenv("NNH_HOST_CXX"), find_on_path));
}

} // namespace nnh::tool
