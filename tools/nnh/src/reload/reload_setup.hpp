#pragma once
// Where a game's assets are (spec 031, plan "Where the assets are"): `nnh_add_assets()` writes, at configure time,
// <binary dir>/nnh_reload_<game>.txt with `assets=` (the source folder), `game=` (the game's source folder) and `out=`
// (the converted folder). ReloadSession owns a run's controller. Tests: tools/nnh/tests/test_reload_tool.cpp.

#include <memory>
#include <optional>
#include <string>

#include "reload/reload_controller.hpp"

namespace nnh::tool::reload {

struct ReloadPaths {
    std::string assets; ///< the source assets folder
    std::string game;   ///< the game's source folder
    std::string out;    ///< the converted assets folder
};

/// Parses the file's text. Empty optional when a line is missing.
std::optional<ReloadPaths> parse_reload_paths(const std::string& text);
/// Reads <binary_dir>/nnh_reload_<game>.txt.
std::optional<ReloadPaths> read_reload_paths(const std::string& binary_dir, const std::string& game);

struct ReloadSession {
    std::unique_ptr<ReloadController> controller;
};

} // namespace nnh::tool::reload
