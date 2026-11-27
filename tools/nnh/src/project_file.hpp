#pragma once
// nnh.toml, the game project marker (spec 032 FR-12, D55): written in the TOML subset nnh already reads for tunes
// dumps (remote::read_tunes). Keys of this spec: `name` and `engine` (text; `engine` is the engine version the project
// was made for, compared by `nnh doctor` in spec 033). Unknown keys (and keys inside [tables]) are one warning line
// each, so later versions can add keys; a malformed file is `nnh.toml:<line>: reason`. Bounded: 64 KiB. Pure; tested
// (and mini-fuzzed) in tools/nnh/tests/test_project_file.cpp and test_fuzz_tool.cpp.

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace nnh::tool {

inline constexpr std::size_t kMaxProjectFileBytes = 64u * 1024u;

struct ProjectFile {
    std::string name;                  ///< `name`, or empty
    std::string engine;                ///< `engine`, or empty
    std::vector<std::string> warnings; ///< "nnh.toml:<line>: unknown key "x" (ignored)"
    std::vector<std::string> errors;   ///< "nnh.toml:<line>: reason"; empty = valid
};

ProjectFile read_project_file(std::string_view text);

} // namespace nnh::tool
