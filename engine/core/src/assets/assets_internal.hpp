#pragma once
// The assets module for the frame loop and other engine modules. Internal.

#include <cstddef>

#include "nnh/memory.hpp"

namespace nnh::detail {

/// The file the build writes next to the assets: one asset path per line (spec 007).
inline constexpr const char* kAssetIndexFile = ".nnh_assets";
inline constexpr std::size_t kMaxAssetIndexBytes = 64 * 1024;

/// Mounts the platform's assets and reads the index (nnh::init()). A game without assets, or with an
/// unreadable index, simply has none (logged); never fails init.
void assets_init();
void assets_shutdown();

/// The stored path of an asset if the game has it (exact match), else nullptr. For load_texture().
const char* asset_lookup(const char* path);

MemTag& assets_memory_tag();

} // namespace nnh::detail
