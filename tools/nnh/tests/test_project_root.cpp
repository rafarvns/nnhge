// The project root (project_root.hpp, spec 032 FR-11): the nearest game project (nnh.toml) or engine repository.

#include <filesystem>
#include <fstream>
#include <string>

#include "project_root.hpp"
#include "test.hpp"

using namespace nnh::tool;

namespace {

namespace fs = std::filesystem;

void touch(const fs::path& file) {
    fs::create_directories(file.parent_path());
    std::ofstream(file) << "";
}

/// <base>/<name>/: an engine repository "engine/" (with a game project "engine/games/inside/"), a game project
/// "game/", nested folders below each.
std::string tree(const fs::path& base, const char* name) {
    const fs::path root = base / name;
    std::error_code ec;
    fs::remove_all(root, ec);
    touch(root / "engine" / "CMakePresets.json");
    fs::create_directories(root / "engine" / "engine" / "core" / "src");
    touch(root / "engine" / "games" / "inside" / "nnh.toml");
    fs::create_directories(root / "engine" / "games" / "inside" / "game" / "src");
    touch(root / "game" / "nnh.toml");
    fs::create_directories(root / "game" / "game" / "assets");
    touch(root / "both" / "nnh.toml"); // a folder with both markers is a game project
    touch(root / "both" / "CMakePresets.json");
    fs::create_directories(root / "both" / "engine" / "core");
    return root.generic_string();
}

} // namespace

NNH_TEST(the_engine_repository_is_found_from_its_root_and_below) {
    const std::string t = tree(NNH_TOOL_TEST_TMP, "root_engine");
    const ProjectRoot at_root = find_project_root(t + "/engine");
    CHECK(at_root.path == t + "/engine");
    CHECK(!at_root.game);
    const ProjectRoot below = find_project_root(t + "/engine/engine/core/src");
    CHECK(below.path == t + "/engine");
    CHECK(!below.game);
}

NNH_TEST(a_game_project_is_found_from_its_root_and_below) {
    const std::string t = tree(NNH_TOOL_TEST_TMP, "root_game");
    const ProjectRoot at_root = find_project_root(t + "/game");
    CHECK(at_root.path == t + "/game");
    CHECK(at_root.game);
    const ProjectRoot below = find_project_root(t + "/game/game/assets");
    CHECK(below.path == t + "/game");
    CHECK(below.game);
}

NNH_TEST(a_game_project_inside_the_engine_repository_is_found_as_itself) {
    const std::string t = tree(NNH_TOOL_TEST_TMP, "root_inside");
    const ProjectRoot r = find_project_root(t + "/engine/games/inside/game/src");
    CHECK(r.path == t + "/engine/games/inside");
    CHECK(r.game);
    CHECK(find_project_root(t + "/engine/games").path == t + "/engine"); // above it: the repository again
}

NNH_TEST(a_folder_with_both_markers_is_a_game_project) {
    const std::string t = tree(NNH_TOOL_TEST_TMP, "root_both");
    const ProjectRoot r = find_project_root(t + "/both/engine/core");
    CHECK(r.path == t + "/both");
    CHECK(r.game);
}

NNH_TEST(the_real_repository_is_found_from_the_tool_sources) {
    const ProjectRoot r = find_project_root(NNH_TOOL_SOURCE_DIR);
    CHECK(!r.path.empty());
    CHECK(!r.game);
    CHECK(find_project_root(r.path).path == r.path);
}

NNH_TEST(nothing_is_found_outside_both) {
    std::error_code ec;
    // The system's temporary folder: no repository above it (the build folder is inside one).
    const fs::path base = fs::temp_directory_path(ec) / "nnh_root_none";
    fs::remove_all(base, ec);
    fs::create_directories(base / "a" / "b", ec);
    const ProjectRoot r = find_project_root((base / "a" / "b").generic_string());
    CHECK(r.path.empty());
    CHECK(!r.game);
    fs::remove_all(base, ec);
}

NNH_TEST(the_message_names_both_markers) {
    const std::string m = no_project_message();
    CHECK(m.find("nnh.toml") != std::string::npos);
    CHECK(m.find("NNHGE repository") != std::string::npos);
    CHECK(m.find("engine/core") != std::string::npos);
}
