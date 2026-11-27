// The repository helpers (repo.hpp, spec 010 FR-2): presets, paths, CMake commands, log names.

#include "repo.hpp"
#include "test.hpp"

using namespace nnh::tool;

NNH_TEST(presets_follow_target_and_flavor) {
    CHECK(preset_name(false, false) == "3ds-dev");
    CHECK(preset_name(false, true) == "3ds-release");
    CHECK(preset_name(true, false) == "pc-dev");
    CHECK(preset_name(true, true) == "pc-release");
}

NNH_TEST(msys_and_native_paths_convert_both_ways) {
    CHECK(native_path("/e/RANDOM/nnhge/build") == "E:/RANDOM/nnhge/build");
    CHECK(native_path("/c") == "C:/");
    CHECK(native_path("E:/RANDOM") == "E:/RANDOM");
    CHECK(native_path("/home/me/nnhge") == "/home/me/nnhge"); // Linux paths stay
    CHECK(native_path("/ee/x") == "/ee/x");
    CHECK(msys_path("E:/RANDOM/nnhge") == "/e/RANDOM/nnhge");
    CHECK(msys_path("E:\\RANDOM\\nnhge") == "/e/RANDOM/nnhge");
    CHECK(msys_path("/home/me") == "/home/me");
}

NNH_TEST(cmake_commands_per_shell) {
    CHECK(cmake_command("/home/me/nnhge", "--build --preset 3ds-dev", Shell{}) ==
          "cd '/home/me/nnhge' && cmake --build --preset 3ds-dev");
    CHECK(cmake_command("E:/nnhge", "--build --preset pc-dev", Shell{true, ""}) ==
          "cd /d \"E:/nnhge\" && cmake --build --preset pc-dev");
    CHECK(cmake_command("E:/nnhge", "--preset 3ds-dev", Shell{true, "C:/devkitPro/msys2/usr/bin/bash.exe"}) ==
          "\"C:/devkitPro/msys2/usr/bin/bash.exe\" -lc \"cd '/e/nnhge' && cmake --preset 3ds-dev\"");
}

NNH_TEST(log_files_are_named_by_game_and_time) {
    std::tm t{};
    t.tm_year = 2026 - 1900;
    t.tm_mon = 10;
    t.tm_mday = 18;
    t.tm_hour = 9;
    t.tm_min = 5;
    t.tm_sec = 7;
    CHECK(log_file_path("E:/nnhge", "hello", t) == "E:/nnhge/build/logs/hello-20261118-090507.txt");
}

NNH_TEST(screenshots_go_in_a_folder_named_like_the_log) {
    CHECK(screenshot_dir("E:/nnhge/build/logs/input-20261122-101500.txt") ==
          "E:/nnhge/build/logs/input-20261122-101500");
    CHECK(screenshot_dir("logs/a.b-1.txt") == "logs/a.b-1");
    CHECK(screenshot_dir("logs/no-extension") == "logs/no-extension.shots"); // never the log file itself
}

NNH_TEST(nnh_preset_picks_another_preset_of_the_same_system) {
    CHECK(preset_for(true, false, nullptr) == "pc-dev");
    CHECK(preset_for(true, false, "") == "pc-dev");
    CHECK(preset_for(true, false, "pc-debug") == "pc-debug");
    CHECK(preset_for(true, true, "pc-debug") == "pc-debug");
    CHECK(preset_for(false, false, "3ds-debug") == "3ds-debug");
    CHECK(preset_for(false, true, "pc-debug") == "3ds-release"); // another system's preset: ignored
    CHECK(preset_for(true, false, "3ds-dev") == "pc-dev");
    CHECK(preset_for(true, false, "pc-") == "pc-dev");
    CHECK(preset_for(true, false, "pc-x y") == "pc-dev"); // goes into a shell command: names only
}
