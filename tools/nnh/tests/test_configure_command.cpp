// The configure command (repo.hpp configure_command, host_compiler.hpp; spec 032 FR-14, FR-15): the host compiler and
// the local engine handed to devkitPro's MSYS CMake in MSYS form; every other command unchanged.

#include <string>

#include "host_compiler.hpp"
#include "repo.hpp"
#include "test.hpp"

using namespace nnh::tool;

namespace {

const Shell kDkp{true, "C:/devkitPro/msys2/usr/bin/bash.exe"};
const Shell kCmd{true, ""};
const Shell kPosix{};

std::string none(const char*) { return ""; }

} // namespace

NNH_TEST(the_host_compiler_comes_from_the_environment_first) {
    CHECK(find_host_cxx("C:/x/g++.exe", [](const char*) { return std::string("C:/y/g++.exe"); }) == "C:/x/g++.exe");
}

NNH_TEST(the_host_compiler_is_searched_on_path_in_dkp_order) {
    CHECK(find_host_cxx(nullptr, [](const char* n) { return std::string(n) == "g++" ? "C:/w/g++.exe" : ""; }) ==
          "C:/w/g++.exe");
    CHECK(find_host_cxx("", [](const char* n) { return std::string(n) == "c++" ? "C:/w/c++.exe" : ""; }) ==
          "C:/w/c++.exe");
    CHECK(find_host_cxx(nullptr, [](const char* n) { return std::string(n) == "clang++" ? "/l/clang++" : ""; }) ==
          "/l/clang++");
    CHECK(find_host_cxx(nullptr, [](const char* n) { return std::string(n) != "clang++" ? "/l/" + std::string(n) : ""; }) ==
          "/l/g++"); // g++ before c++
}

NNH_TEST(no_host_compiler_gives_an_empty_value) {
    CHECK(find_host_cxx(nullptr, none).empty());
    CHECK(find_host_cxx("", none).empty());
}

NNH_TEST(a_3ds_configure_on_windows_passes_msys_paths) {
    CHECK(configure_command("E:/games/my game", "3ds-dev", kDkp,
                            {.host_cxx = "C:\\WinLibs\\bin\\g++.exe", .engine_dir = "e:/RANDOM/nnhge"}) ==
          "\"C:/devkitPro/msys2/usr/bin/bash.exe\" -lc \"cd '/e/games/my game' && cmake --preset 3ds-dev "
          "'-DNNH_HOST_CXX=/c/WinLibs/bin/g++.exe' '-DFETCHCONTENT_SOURCE_DIR_NNHGE=/e/RANDOM/nnhge'\"");
}

NNH_TEST(a_3ds_configure_on_windows_without_extras_is_the_plain_command) {
    CHECK(configure_command("E:/nnhge", "3ds-release", kDkp, {}) ==
          "\"C:/devkitPro/msys2/usr/bin/bash.exe\" -lc \"cd '/e/nnhge' && cmake --preset 3ds-release\"");
    CHECK(configure_command("E:/nnhge", "3ds-dev", kDkp, {.host_cxx = "c:/Program Files/g++.exe", .engine_dir = ""}) ==
          "\"C:/devkitPro/msys2/usr/bin/bash.exe\" -lc \"cd '/e/nnhge' && cmake --preset 3ds-dev "
          "'-DNNH_HOST_CXX=/c/Program Files/g++.exe'\"");
}

NNH_TEST(a_quote_in_a_path_is_refused) {
    CHECK(configure_command("E:/nnhge", "3ds-dev", kDkp, {.host_cxx = "C:/it's/g++.exe", .engine_dir = ""}).empty());
    CHECK(configure_command("E:/nnhge", "3ds-dev", kDkp, {.host_cxx = "", .engine_dir = "E:/a\"b"}).empty());
    CHECK(configure_command("E:/it's", "3ds-dev", kDkp, {}).empty());
}

NNH_TEST(pc_and_linux_configures_are_unchanged) {
    const ConfigureExtras extras{.host_cxx = "C:/x/g++.exe", .engine_dir = "E:/nnhge"};
    CHECK(configure_command("E:/game", "pc-debug", kCmd, extras) == "cd /d \"E:/game\" && cmake --preset pc-debug");
    CHECK(configure_command("/home/me/game", "3ds-dev", kPosix, extras) == "cd '/home/me/game' && cmake --preset 3ds-dev");
    CHECK(configure_command("/home/me/game", "pc-dev", kPosix, extras) == "cd '/home/me/game' && cmake --preset pc-dev");
}
