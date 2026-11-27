// The glue of commands/project.cpp (spec 032 AC-6): the project opened, its nnh.toml read, its presets checked before
// CMake runs (configure_and_list too: nnh serve lists games through it).

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "commands.hpp"
#include "test.hpp"

using namespace nnh::tool;

namespace {

namespace fs = std::filesystem;

/// <tmp>/<name>/ holding nnh.toml (`toml`) and a CMakePresets.json defining only pc-dev.
std::string project(const char* name, const char* toml) {
    const fs::path root = fs::path(NNH_TOOL_TEST_TMP) / name;
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root / "game");
    std::ofstream(root / "nnh.toml") << toml;
    std::ofstream(root / "CMakePresets.json")
        << R"({ "version": 6, "configurePresets": [ { "name": "pc-dev" } ],
               "buildPresets": [ { "name": "pc-dev", "configurePreset": "pc-dev" } ] })";
    return root.generic_string();
}

struct Captured {
    std::vector<std::string> lines;
    OutputFn fn = [this](const std::string& s) {
        lines.push_back(s);
        return true;
    };
    bool has(const std::string& text) const {
        for (const std::string& l : lines) {
            if (l.find(text) != std::string::npos) return true;
        }
        return false;
    }
};

} // namespace

NNH_TEST(a_missing_preset_is_named_before_cmake_runs) {
    const std::string root = project("cmd_preset", "");
    Captured out;
    CHECK(check_preset(root, "pc-dev", &out.fn));
    CHECK(out.lines.empty());
    CHECK(!check_preset(root, "3ds-dev", &out.fn));
    CHECK(out.has("nnh: this project has no preset \"3ds-dev\" (CMakePresets.json)"));
}

NNH_TEST(configure_and_list_checks_the_preset_first) {
    const std::string root = project("cmd_list", "");
    Captured out;
    std::vector<GameEntry> games;
    CHECK(!configure_and_list(root, "3ds-dev", true, games, &out.fn));
    CHECK(out.has("no preset \"3ds-dev\""));
    CHECK(!out.has("configuring")); // CMake never ran
    CHECK(!fs::exists(root + "/build"));
}

NNH_TEST(a_malformed_project_file_stops_the_command) {
    const std::string root = project("cmd_bad_toml", "name = \"ok\"\nthis is not toml\n");
    Captured out;
    CHECK(open_project_at(root + "/game", &out.fn).empty());
    CHECK(out.has("nnh: nnh.toml:2: expected \"name = value\""));
}

NNH_TEST(a_project_files_warnings_are_shown_and_it_opens) {
    const std::string root = project("cmd_warn_toml", "name = \"ok\"\ncolour = \"red\"\n");
    Captured out;
    CHECK(open_project_at(root + "/game", &out.fn) == root);
    CHECK(out.has("nnh: nnh.toml:2: unknown key \"colour\" (ignored)"));
}
