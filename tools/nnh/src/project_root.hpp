#pragma once
// The folder nnh works in (spec 032 FR-11, D55): the nearest game project (a folder holding nnh.toml) or NNHGE
// repository (CMakePresets.json + engine/core/) at or above a starting folder. Tested in
// tools/nnh/tests/test_project_root.cpp.

#include <string>

namespace nnh::tool {

struct ProjectRoot {
    std::string path;  ///< generic form ("E:/x/y", "/home/me/x"); empty when nothing was found
    bool game = false; ///< true: a game project (nnh.toml); false: the engine repository
};

/// Walks up from `start` (at most 64 levels) and stops at the first folder that is a game project or the engine
/// repository; a folder with both markers is a game project. Reads only the file system's entries.
ProjectRoot find_project_root(const std::string& start);

/// The message when find_project_root found nothing (without a trailing newline).
std::string no_project_message();

} // namespace nnh::tool
