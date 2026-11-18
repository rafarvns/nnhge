#pragma once
// Asset path rules (spec 007 FR-6). Pure; unit tested in tests/assets/test_asset_path.cpp.
//
// An asset path is relative to the game's assets folder, '/'-separated, without "." or ".." segments or
// empty segments, at most kMaxAssetPath bytes, in a portable character set (ASCII letters, digits,
// space, '-', '_', '.', '/'). The build refuses asset file names that break these rules, so a valid
// path means the same file on every system.

#include <cstddef>

namespace nnh::detail {

inline constexpr std::size_t kMaxAssetPath = 255;

bool valid_asset_path(const char* path);

} // namespace nnh::detail
