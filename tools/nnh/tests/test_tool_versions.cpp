// Tool versions from recorded outputs (doctor/tool_versions.hpp, spec 033 FR-19, AC-6). The outputs in
// tools/nnh/tests/data/doctor/ were recorded from the real tools (see its README.md).

#include "doctor/tool_versions.hpp"
#include "test.hpp"

#include <fstream>
#include <iterator>
#include <string>

using namespace nnh::tool;

namespace {

std::string recorded(const char* name) {
    std::ifstream in(std::string(NNH_TOOL_SOURCE_DIR "/tests/data/doctor/") + name, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

bool is(const Version& v, int major, int minor, int patch) {
    return v.major == major && v.minor == minor && v.patch == patch;
}

} // namespace

NNH_TEST(cmake_and_ninja_versions_are_read) {
    CHECK(is(cmake_version(recorded("cmake-3.25.1.txt")), 3, 25, 1));
    CHECK(is(cmake_version(recorded("cmake-3.20.0.txt")), 3, 20, 0));
    CHECK(is(cmake_version(recorded("cmake-3.28.3-ubuntu.txt")), 3, 28, 3));
    CHECK(is(cmake_version(recorded("cmake-4.4.2-winlibs.txt")), 4, 4, 2));
    CHECK(is(cmake_version(recorded("cmake-4.0.2-devkitpro-msys2.txt")), 4, 0, 2));
    CHECK(is(ninja_version(recorded("ninja-1.13-winlibs.txt")), 1, 13, 2));
    CHECK(is(ninja_version(recorded("ninja-1.11.1-ubuntu.txt")), 1, 11, 1));
    CHECK(!cmake_version("").valid() && !cmake_version("cmake: command not found").valid());
    CHECK(!ninja_version("ninja: error").valid());
}

NNH_TEST(gcc_banners_of_every_packaging_are_read) {
    CompilerVersion c = compiler_version(recorded("gcc-16.2-winlibs.txt"));
    CHECK(c.kind == CompilerKind::Gcc && is(c.version, 16, 2, 0));
    c = compiler_version(recorded("gcc-13.3-ubuntu.txt"));
    CHECK(c.kind == CompilerKind::Gcc &&
          is(c.version, 13, 3, 0)); // not the package's "13.3.0-6ubuntu2~24.04.1"
    c = compiler_version(recorded("gcc-12.4-ubuntu.txt"));
    CHECK(c.kind == CompilerKind::Gcc && is(c.version, 12, 4, 0));
    c = compiler_version(recorded("arm-none-eabi-gcc-devkitarm-r68.txt"));
    CHECK(c.kind == CompilerKind::Gcc && is(c.version, 16, 1, 0));
    c = compiler_version("c++ (GCC) 14.2.1 20240910\nCopyright (C) 2024 Free Software Foundation, Inc.\n");
    CHECK(c.kind == CompilerKind::Gcc && is(c.version, 14, 2, 1));
}

NNH_TEST(clang_apple_clang_and_msvc_banners_are_read) {
    CompilerVersion c = compiler_version(recorded("clang-18.1-ubuntu.txt"));
    CHECK(c.kind == CompilerKind::Clang && is(c.version, 18, 1, 3));
    c = compiler_version(recorded("clang-16.0-ubuntu.txt"));
    CHECK(c.kind == CompilerKind::Clang && is(c.version, 16, 0, 6));
    c = compiler_version(recorded("apple-clang-15.0.txt"));
    CHECK(c.kind == CompilerKind::AppleClang && is(c.version, 15, 0, 0));
    c = compiler_version(recorded("msvc-cl-19.44.txt"));
    CHECK(c.kind == CompilerKind::Msvc && is(c.version, 19, 44, 35211));
    c = compiler_version("clang version 19.1.0 (https://github.com/llvm/llvm-project.git 9b0e41a3)\n");
    CHECK(c.kind == CompilerKind::Clang && is(c.version, 19, 1, 0));
    CHECK(compiler_version("").kind == CompilerKind::Unknown);
    CHECK(compiler_version("something else 1.2.3\n").kind == CompilerKind::Unknown);
}

NNH_TEST(versions_compare_and_print) {
    CHECK((Version{3, 25, 0} < Version{3, 25, 1} && Version{3, 9, 9} < Version{3, 25, 0}));
    CHECK((Version{12, 0, 0} >= Version{12, 0, 0} && Version{4, 0, 0} > Version{3, 99, 99}));
    CHECK((Version{3, 25, 1}.text() == "3.25.1" && Version{}.text().empty()));
    CHECK(is(first_version("x 1.2 y"), 1, 2, 0));
    CHECK(!first_version("1. 2 3").valid() && !first_version("v12").valid());
    CHECK(!first_version("9999999.1.1").valid()); // numbers are bounded
}

NNH_TEST(pacman_folder_names_give_package_versions) {
    const std::vector<std::string> windows = {
        "3ds-cmake-1.5.2-1", "citro2d-1.7.0-1",      "citro3d-1.7.1-2",
        "devkitARM-r68-1",   "devkitARM-gdb-14.1-1", "devkitarm-gcc-16.1.0-1",
        "libctru-2.7.1-1",   "ninja-1.13.2-2",       "cmake-4.0.2-1"};
    CHECK(pacman_version(windows, "libctru") == "2.7.1");
    CHECK(pacman_version(windows, "citro2d") == "1.7.0");
    CHECK(pacman_version(windows, "citro3d") == "1.7.1");
    CHECK(pacman_version(windows, "devkitARM") == "r68"); // not devkitARM-gdb
    CHECK(pacman_version(windows, "cmake") == "4.0.2");   // not 3ds-cmake
    CHECK(pacman_version(windows, "libnds").empty());
    CHECK(pacman_version({"libctru"}, "libctru").empty());
    CHECK(pacman_version({}, "libctru").empty());
}
