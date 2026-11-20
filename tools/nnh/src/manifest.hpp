#pragma once
// The games manifest (spec 010): nnh_add_game() writes one line per game into <build>/nnh_games.txt —
// "name|binary dir|cia|title" (cia 1 or 0; the title, since spec 011, is optional and may contain '|') — so nnh can turn a game name into its targets and outputs. Pure;
// unit tested in tools/nnh/tests/test_manifest.cpp.

#include <optional>
#include <string>
#include <vector>

namespace nnh::tool {

struct GameEntry {
    std::string name;       ///< the CMake target
    std::string binary_dir; ///< where its outputs are (<name>.3dsx, <name>.exe, ...)
    bool cia = false;       ///< nnh_add_game(... CIA ...)
    std::string title;      ///< its TITLE (the name when the line has none)
};

/// Parses the manifest text; malformed lines are skipped, a later line for the same game wins.
std::vector<GameEntry> parse_manifest(const std::string& text);

std::optional<GameEntry> find_game(const std::vector<GameEntry>& games, const std::string& name);

} // namespace nnh::tool
