#pragma once
/// @file nnh/platform/assets.hpp
/// Platform contract — the game's read-only assets (spec 007). Implemented by every platform library,
/// called only by nnh::core. Not for games. See docs/architecture/platforms.md.
///
/// Core calls these only with paths that passed its rules (relative, '/'-separated, no dot segments,
/// portable characters) and that are listed in the asset index, so a platform needs no checks of its
/// own beyond opening and reading the file.

#include <cstddef>

namespace nnh::platform {

/// Makes the assets readable (3DS: mount the romfs; PC: find the assets folder next to the program).
/// @return false if the game has no assets. Called once by nnh::init().
bool assets_mount();

/// Undoes assets_mount() (nnh::shutdown()). Safe if mounting failed.
void assets_unmount();

/// Size in bytes of an asset. @return false if it can't be opened.
bool asset_size(const char* path, std::size_t& out);

/// Reads a whole asset into `buffer` (`capacity` >= its size). @return false on any failure; `read`
/// receives the bytes read. Load time only.
bool asset_read(const char* path, void* buffer, std::size_t capacity, std::size_t& read);

} // namespace nnh::platform
