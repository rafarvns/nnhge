#pragma once
// Reading tool versions from what the tools print (spec 033 FR-19, D56 item 5): `cmake --version`, `ninja --version`,
// a C++ compiler's `--version` (GCC, MinGW-w64/WinLibs, clang, Apple clang) or MSVC `cl`'s banner,
// `arm-none-eabi-gcc --version`, and devkitPro's pacman package folder names. Bounded (only the first 64 KiB are read,
// numbers of at most 6 digits) and pure; tested on recorded outputs (tools/nnh/tests/data/doctor/) and mini-fuzzed in
// tools/nnh/tests/test_tool_versions.cpp and test_fuzz_tool.cpp.

#include <compare>
#include <string>
#include <string_view>
#include <vector>

namespace nnh::tool {

struct Version {
    int major = -1; ///< -1 = none found
    int minor = 0;
    int patch = 0;
    bool valid() const { return major >= 0; }
    auto operator<=>(const Version&) const = default;
    std::string text() const; ///< "3.25.1"; "" when invalid
};

/// The first "<n>.<n>[.<n>]" in `text` after position `from`.
Version first_version(std::string_view text, std::size_t from = 0);

Version cmake_version(std::string_view output); ///< "cmake version 3.25.1"
Version ninja_version(std::string_view output); ///< "1.11.1"

enum class CompilerKind { Unknown, Gcc, Clang, AppleClang, Msvc };

struct CompilerVersion {
    CompilerKind kind = CompilerKind::Unknown;
    Version version;
};

/// A compiler's `--version` output (or `cl`'s banner, which it prints on stderr).
CompilerVersion compiler_version(std::string_view output);

/// The version of package `name` in pacman's database folder names ("libctru-2.7.1-1" -> "2.7.1",
/// "devkitARM-r68-1" -> "r68"); "" when no folder is that package.
std::string pacman_version(const std::vector<std::string>& folder_names, std::string_view name);

} // namespace nnh::tool
