#pragma once
// nnh's per-user settings (spec 010 FR-4): "key=value" lines in %APPDATA%/nnhge/nnh.ini (Windows) or
// $XDG_CONFIG_HOME/nnhge/nnh.ini (~/.config/nnhge/nnh.ini). Keys: `console` (the last console used), `server_id` (nnh serve's id, spec 011).
// Parsing is pure and unit tested in tools/nnh/tests/test_user_config.cpp.

#include <map>
#include <string>

namespace nnh::tool {

using Settings = std::map<std::string, std::string>;

Settings parse_settings(const std::string& text);
std::string format_settings(const Settings& settings);

/// The settings file's path from the environment (`appdata`, `xdg_config_home`, `home`; empty = unset).
/// Empty if none is set.
std::string settings_path(const std::string& appdata, const std::string& xdg_config_home, const std::string& home);

/// The current user's settings file ("" if the environment names no folder).
std::string user_settings_path();

/// Reads / writes the settings file of the current user (missing file = no settings).
Settings load_user_settings();
bool save_user_settings(const Settings& settings);

} // namespace nnh::tool
