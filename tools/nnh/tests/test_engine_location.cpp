// Which engine a new project uses (new/engine_location.hpp, spec 033 FR-9, FR-10).

#include "build_info.hpp"
#include "new/engine_location.hpp"
#include "test.hpp"

#include <filesystem>
#include <fstream>
#include <string>

using namespace nnh::tool;

namespace {

BuildInfo checkout(const std::string& dir) {
    BuildInfo info;
    info.version = "0.1.0";
    info.checkout_dir = dir;
    return info;
}

BuildInfo release() {
    BuildInfo info;
    info.version = "0.1.0";
    info.engine_url = "https://github.com/rafarvns/nnhge/archive/refs/tags/v0.1.0.tar.gz";
    info.engine_sha256 = std::string(64, 'a');
    return info;
}

/// Engine folders: E:/src/nnhge (0.1.0) and E:/other (0.2.0); anything else is not one.
std::string fake_version(const std::string& dir) {
    if (dir == "E:/src/nnhge" || dir == "/home/me/nnhge") return "0.1.0";
    if (dir == "E:/other") return "0.2.0";
    return "";
}

std::string joined(const std::vector<std::string>& lines) {
    std::string all;
    for (const std::string& l : lines)
        all += l + "\n";
    return all;
}

bool has(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

} // namespace

NNH_TEST(a_checkout_nnh_uses_its_own_checkout_and_prints_the_set_lines) {
    const EngineChoice c = choose_engine(checkout("E:/src/nnhge"), "", nullptr, true, fake_version);
    CHECK(c.ok && c.local_dir == "E:/src/nnhge" && c.local_version == "0.1.0");
    CHECK(c.engine_url.empty() && c.engine_sha256.empty()); // no pin: the guard
    const std::string out = joined(c.notes);
    CHECK(has(out, "local engine E:/src/nnhge"));
    CHECK(has(out, "export NNHGE_ENGINE_DIR=E:/src/nnhge"));
    CHECK(has(out, "$env:NNHGE_ENGINE_DIR = \"E:/src/nnhge\""));
    CHECK(has(out, "set NNHGE_ENGINE_DIR=E:/src/nnhge"));
    CHECK(has(out, "setx NNHGE_ENGINE_DIR E:/src/nnhge"));
}

NNH_TEST(linux_prints_export_and_the_profile_hint) {
    const EngineChoice c = choose_engine(checkout("/home/me/nnhge"), "", nullptr, false, fake_version);
    const std::string out = joined(c.notes);
    CHECK(c.ok && has(out, "export NNHGE_ENGINE_DIR=/home/me/nnhge") && has(out, "~/.profile"));
    CHECK(!has(out, "setx") && !has(out, "PowerShell"));
}

NNH_TEST(the_engine_option_replaces_the_checkout_and_must_be_an_engine) {
    EngineChoice c = choose_engine(checkout("E:/src/nnhge"), "E:/other", nullptr, true, fake_version);
    CHECK(c.ok && c.local_dir == "E:/other" && c.local_version == "0.2.0");
    c = choose_engine(checkout("E:/src/nnhge"), "E:/nothing", nullptr, true, fake_version);
    CHECK(!c.ok && has(c.error, "--engine E:/nothing is not an NNHGE engine folder"));
    c = choose_engine(release(), "E:/nothing", nullptr, true, fake_version);
    CHECK(!c.ok && has(c.error, "--engine"));
}

NNH_TEST(a_moved_checkout_is_refused_with_a_hint) {
    const EngineChoice c = choose_engine(checkout("E:/gone"), "", nullptr, true, fake_version);
    CHECK(!c.ok && has(c.error, "E:/gone") && has(c.error, "pass --engine"));
    const EngineChoice none = choose_engine(checkout(""), "", nullptr, true, fake_version);
    CHECK(!none.ok && has(none.error, "--engine"));
}

NNH_TEST(the_set_lines_are_skipped_when_the_variable_already_names_the_folder) {
    for (const char* env : {"E:/src/nnhge", "e:\\src\\nnhge\\", "E:/SRC/NNHGE/"}) {
        const EngineChoice c = choose_engine(checkout("E:/src/nnhge"), "", env, true, fake_version);
        const std::string out = joined(c.notes);
        CHECK(c.ok && !has(out, "export ") && has(out, "NNHGE_ENGINE_DIR already names it"));
    }
    // Linux paths are case-sensitive
    const EngineChoice l =
        choose_engine(checkout("/home/me/nnhge"), "", "/home/me/NNHGE", false, fake_version);
    CHECK(has(joined(l.notes), "export NNHGE_ENGINE_DIR=/home/me/nnhge"));
}

NNH_TEST(another_folder_in_the_variable_is_named) {
    const EngineChoice c = choose_engine(checkout("E:/src/nnhge"), "", "E:/other", true, fake_version);
    const std::string out = joined(c.notes);
    CHECK(c.ok &&
          has(out, "NNHGE_ENGINE_DIR is set to E:/other: builds of this project will use that engine"));
    CHECK(has(out, "export NNHGE_ENGINE_DIR=E:/src/nnhge"));
}

NNH_TEST(a_release_nnh_pins_its_archive_and_needs_no_variable) {
    const EngineChoice c = choose_engine(release(), "", nullptr, true, fake_version);
    CHECK(c.ok && c.engine_url == release().engine_url && c.engine_sha256 == release().engine_sha256);
    CHECK(c.local_dir.empty());
    CHECK(!has(joined(c.notes), "export "));
    const EngineChoice set = choose_engine(release(), "", "E:/other", true, fake_version);
    CHECK(has(joined(set.notes), "NNHGE_ENGINE_DIR is set to E:/other") &&
          set.engine_url == release().engine_url);
    const EngineChoice local = choose_engine(release(), "E:/src/nnhge", nullptr, true, fake_version);
    CHECK(local.ok && local.local_dir == "E:/src/nnhge" && local.engine_url == release().engine_url);
    CHECK(has(joined(local.notes), "export NNHGE_ENGINE_DIR=E:/src/nnhge"));
}

NNH_TEST(paths_with_spaces_are_quoted_in_every_shell) {
    const auto lines = joined(env_lines("E:/my src/nnhge", true));
    CHECK(has(lines, "export NNHGE_ENGINE_DIR=\"E:/my src/nnhge\""));
    CHECK(has(lines, "$env:NNHGE_ENGINE_DIR = \"E:/my src/nnhge\""));
    CHECK(has(lines, "set \"NNHGE_ENGINE_DIR=E:/my src/nnhge\""));
    CHECK(has(lines, "setx NNHGE_ENGINE_DIR \"E:/my src/nnhge\""));
}

NNH_TEST(the_engine_option_accepts_msys_and_relative_forms) {
    CHECK(engine_dir_from_option("/e/src/nnhge", "C:/work", true) == "E:/src/nnhge");
    CHECK(engine_dir_from_option("E:\\src\\nnhge\\", "C:/work", true) == "E:/src/nnhge");
    CHECK(engine_dir_from_option("../nnhge", "C:/work/games", true) == "C:/work/nnhge");
    CHECK(engine_dir_from_option("nnhge", "/home/me", false) == "/home/me/nnhge");
    CHECK(engine_dir_from_option("/home/me/nnhge/", "/tmp", false) == "/home/me/nnhge");
}

NNH_TEST(the_engine_version_is_read_from_its_top_level_project) {
    CHECK(engine_version_from_cmakelists(
              "cmake_minimum_required(VERSION 3.25)\n\nproject(nnhge VERSION 0.1.0 LANGUAGES C CXX)\n") ==
          "0.1.0");
    CHECK(engine_version_from_cmakelists("project(nnhge VERSION 12.34.56)") == "12.34.56");
    CHECK(engine_version_from_cmakelists("project(mygame VERSION 0.1.0)").empty());
    CHECK(engine_version_from_cmakelists("# project(nnhge VERSION 0.1.0)\n").empty());
    CHECK(engine_version_from_cmakelists("project(nnhge VERSION x.y)").empty());
    CHECK(engine_version_from_cmakelists("").empty());
}

NNH_TEST(an_engine_folder_needs_its_project_and_the_game_module) {
    namespace fs = std::filesystem;
    const fs::path root = fs::path(NNH_TOOL_TEST_TMP) / "engine_folder";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root / "cmake");
    std::ofstream(root / "CMakeLists.txt") << "project(nnhge VERSION 0.3.1 LANGUAGES C CXX)\n";
    CHECK(engine_folder_version(root.generic_string()).empty()); // no cmake/nnh_game.cmake yet
    std::ofstream(root / "cmake" / "nnh_game.cmake") << "\n";
    CHECK(engine_folder_version(root.generic_string()) == "0.3.1");
    CHECK(engine_folder_version((root / "missing").generic_string()).empty());
    CHECK(engine_folder_version(NNH_TOOL_SOURCE_DIR "/../..") == build_info().version); // this checkout
}
