// nnh doctor's This project group on fake machines (doctor/checks_project.cpp, spec 033 FR-19, AC-6).

#include "doctor/checks.hpp"
#include "doctor/project_versions.hpp"
#include "doctor_checks_util.hpp"
#include "test.hpp"

using namespace nnh::tool;
using namespace nnh::tool::test;

namespace {

const char* const kPresets =
    R"({"version": 6, "configurePresets": [{"name": "pc-dev"}, {"name": "pc-release"},
  {"name": "3ds-dev"}, {"name": "3ds-release"}], "buildPresets": [{"name": "pc-dev"}, {"name": "pc-release"},
  {"name": "3ds-dev"}, {"name": "3ds-release"}]})";

/// A game project in E:/games/demo made by nnh new (no pin).
FakeMachine project_machine() {
    FakeMachine m = windows_machine();
    m.file_text["E:/games/demo/nnh.toml"] = "name = \"demo\"\nengine = \"0.1.0\"\n";
    m.file_text["E:/games/demo/CMakePresets.json"] = kPresets;
    m.file_text["E:/games/demo/CMakeLists.txt"] = "set(nnhge_url \"\")\nset(nnhge_sha256 \"\")\n";
    return m;
}

DoctorReport project(const FakeMachine& m, bool game = true) {
    DoctorContext c = context(Host::Windows);
    c.project_root = "E:/games/demo";
    c.game_project = game;
    DoctorReport r;
    check_project(m.probes(), c, r);
    return r;
}

} // namespace

NNH_TEST(a_fresh_project_is_ready) {
    const DoctorReport r = project(project_machine());
    CHECK(check(r, "This project", "nnh.toml").state == CheckState::Ok);
    CHECK(check(r, "This project", "nnh.toml").found == "E:/games/demo (demo)");
    const DoctorCheck engine = check(r, "This project", "engine");
    CHECK(engine.state == CheckState::Ok && engine.found == "0.1.0 (nnh.toml; this nnh: 0.1.0)");
    CHECK(check(r, "This project", "presets").state == CheckState::Ok);
    CHECK(check(r, "This project", "NNHGE_ENGINE_DIR").found == "not set (the project's pinned engine)");
    CHECK(check(r, "This project", "disk").state == CheckState::Ok);
    CHECK(doctor_exit_code(r, false) == 0);
}

NNH_TEST(outside_a_project_the_group_is_not_checked) {
    DoctorContext c = context(Host::Windows);
    DoctorReport r;
    check_project(project_machine().probes(), c, r);
    CHECK(group(r, "This project") &&
          has(group(r, "This project")->not_checked, "not inside a game project"));
}

NNH_TEST(another_engine_version_warns_with_the_fix) {
    FakeMachine m = project_machine();
    m.file_text["E:/games/demo/nnh.toml"] = "name = \"demo\"\nengine = \"0.2.0\"\n";
    DoctorCheck e = check(project(m), "This project", "engine");
    CHECK(e.state == CheckState::Warn && has(e.found, "0.2.0") && has(e.fix, "use the nnh of NNHGE 0.2.0"));
    CHECK(has(e.fix, "pin NNHGE 0.1.0"));
    // A configure's cache wins over nnh.toml: the engine really built
    m.file_text["E:/games/demo/build/pc-dev/CMakeCache.txt"] = "FOO:BOOL=ON\nNNHGE_VERSION:INTERNAL=0.1.0\n";
    e = check(project(m), "This project", "engine");
    CHECK(e.state == CheckState::Ok && has(e.found, "build/pc-dev"));
    m.file_text["E:/games/demo/build/pc-dev/CMakeCache.txt"] = "NNHGE_VERSION:STRING=0.3.0\n";
    CHECK(check(project(m), "This project", "engine").state == CheckState::Warn);
}

NNH_TEST(the_engine_repository_reports_its_own_version) {
    FakeMachine m = windows_machine();
    m.file_text["E:/games/demo/CMakeLists.txt"] =
        "cmake_minimum_required(VERSION 3.25)\nproject(nnhge VERSION 0.1.0 LANGUAGES C CXX)\n";
    m.file_text["E:/games/demo/CMakePresets.json"] = kPresets;
    const DoctorReport r = project(m, false);
    CHECK(check(r, "This project", "nnh.toml").name == "<missing>");
    CHECK(check(r, "This project", "engine").found == "0.1.0 (CMakeLists.txt; this nnh: 0.1.0)");
}

NNH_TEST(a_malformed_nnh_toml_fails_and_unknown_keys_warn) {
    FakeMachine m = project_machine();
    m.file_text["E:/games/demo/nnh.toml"] = "name = \"demo\"\nnot toml\n";
    DoctorCheck t = check(project(m), "This project", "nnh.toml");
    CHECK(t.state == CheckState::Fail && has(t.found, "nnh.toml:2") && has(t.fix, "game-projects.md"));
    m.file_text["E:/games/demo/nnh.toml"] = "name = \"demo\"\nengine = \"0.1.0\"\ncolour = \"blue\"\n";
    t = check(project(m), "This project", "nnh.toml");
    CHECK(t.state == CheckState::Warn && has(t.found, "unknown key"));
}

NNH_TEST(a_missing_preset_fails) {
    FakeMachine m = project_machine();
    m.file_text["E:/games/demo/CMakePresets.json"] =
        R"({"configurePresets": [{"name": "pc-dev"}], "buildPresets": [{"name": "pc-dev"}]})";
    DoctorCheck p = check(project(m), "This project", "presets");
    CHECK(p.state == CheckState::Fail && has(p.found, "pc-release") && has(p.fix, "game-projects.md"));
    m.file_text["E:/games/demo/CMakePresets.json"] = kPresets;
    m.env["NNH_PRESET"] = "pc-debug";
    p = check(project(m), "This project", "presets");
    CHECK(p.state == CheckState::Fail && has(p.found, "pc-debug"));
}

NNH_TEST(the_local_engine_variable_is_checked) {
    FakeMachine m = project_machine();
    m.env["NNHGE_ENGINE_DIR"] = "E:/src/nnhge";
    m.file_text["E:/src/nnhge/CMakeLists.txt"] = "project(nnhge VERSION 0.1.0 LANGUAGES C CXX)\n";
    m.files.insert("E:/src/nnhge/cmake/nnh_game.cmake");
    DoctorCheck v = check(project(m), "This project", "NNHGE_ENGINE_DIR");
    CHECK(v.state == CheckState::Ok && v.found == "E:/src/nnhge (NNHGE 0.1.0)");
    m.file_text["E:/games/demo/CMakeLists.txt"] = "set(nnhge_url \"https://x/nnhge-0.1.0.tar.gz\")\n";
    v = check(project(m), "This project", "NNHGE_ENGINE_DIR");
    CHECK(v.state == CheckState::Warn && has(v.found, "not its pinned release") &&
          has(v.fix, "a-local-engine"));
    m.env["NNHGE_ENGINE_DIR"] = "E:/nothing";
    v = check(project(m), "This project", "NNHGE_ENGINE_DIR");
    CHECK(v.state == CheckState::Fail && has(v.found, "not an NNHGE engine folder"));
}

NNH_TEST(low_disk_space_warns_then_fails) {
    FakeMachine m = project_machine();
    m.free = 300ll * 1000 * 1000;
    CHECK(check(project(m), "This project", "disk").state == CheckState::Warn);
    m.free = 50ll * 1000 * 1000;
    const DoctorCheck d = check(project(m), "This project", "disk");
    CHECK(d.state == CheckState::Fail && has(d.found, "50 MB") && !d.fix.empty());
}

NNH_TEST(the_cache_reader_finds_the_engine_version) {
    CHECK(cache_engine_version("A:BOOL=1\nNNHGE_VERSION:INTERNAL=0.1.0\nB=2\n") == "0.1.0");
    CHECK(cache_engine_version("NNHGE_VERSION:STRING=1.2.3\r\n") == "1.2.3");
    CHECK(cache_engine_version("//NNHGE_VERSION:STRING=9.9.9\nNNHGE_VERSION_MAJOR:INTERNAL=0\n").empty());
    CHECK(cache_engine_version("NNHGE_VERSION:INTERNAL=x.y\n").empty());
    CHECK(cache_engine_version("").empty());
    CHECK(project_pin_url("set(nnhge_url \"https://a/b.tar.gz\")\n") == "https://a/b.tar.gz");
    CHECK(project_pin_url("set(nnhge_url \"\")\n").empty());
    CHECK(project_pin_url("FetchContent_Declare(nnhge URL https://x)\n")
              .empty()); // only nnh new's form is read
}

NNH_TEST(the_engine_variable_in_msys_form_and_in_the_repository) { // review 033
    FakeMachine m = project_machine();
    m.env["NNHGE_ENGINE_DIR"] = "/e/src/nnhge";
    const DoctorCheck v = check(project(m), "This project", "NNHGE_ENGINE_DIR");
    CHECK(v.state == CheckState::Fail && has(v.found, "write it in drive form: E:/src/nnhge"));
    m.env.erase("NNHGE_ENGINE_DIR");
    m.file_text["E:/games/demo/CMakeLists.txt"] = "project(nnhge VERSION 0.1.0 LANGUAGES C CXX)\n";
    CHECK(check(project(m, false), "This project", "NNHGE_ENGINE_DIR").found == "not set");
}
