#pragma once
/// @file nnh/platform/reload.hpp
/// Platform contract — hot reload of assets (spec 031, D53). Debug/Development only: core calls these from discarded
/// branches in Release, so a Release game links none of them. Implemented by every platform library in its own
/// `reload_<system>.cpp`. Not for games.
///
/// Both functions return an AssetStream that asset_read_at() and asset_close() (nnh/platform/assets.hpp) accept.

#include "nnh/platform/assets.hpp"

namespace nnh::platform {

/// Opens a file of the game's data folder (a path nnh::fs validated, e.g. ".nnh_reload/12.bin") as an asset stream:
/// a received asset that replaces the packed one. @return false if it can't be opened.
bool reload_stream_open(const char* data_path, AssetStream& out);

/// Opens a packed asset like asset_open(), but so that another program may rewrite or replace the file while it is
/// open (Windows: every share flag; elsewhere the plain open: POSIX and the read-only romfs need nothing more). Used
/// for streams kept open across frames (music), so the build can reconvert a file the simulator streams (AC-9).
bool asset_open_shared(const char* path, AssetStream& out);

} // namespace nnh::platform
