#pragma once
/// @file nnh/assets.hpp
/// The game's read-only assets: the files of the folder given to nnh_add_game(... ASSETS <dir>),
/// converted by the build and packed with the game (3DS romfs, a folder next to the PC program). Paths
/// are relative to that folder, '/'-separated and case-sensitive on every system. See docs/api/assets.md.
///
/// @code
/// auto level = nnh::assets::load("levels/level1.txt");
/// if (!level) { NNH_LOG_ERROR(game, "%s", level.error().message); return; }
/// parse_level(static_cast<const char*>(level.value().data()), level.value().size());
/// @endcode
/// Textures load from assets too: nnh::load_texture("characters/hero") (<nnh/texture.hpp>).
///
/// Hot reload (spec 031, Debug/Development builds under `nnh run`): saving a file in the game's assets folder reloads
/// it at the start of a frame. Textures, sounds, music, fonts, meshes, sprite libraries and string tables change behind
/// the handles the game holds; data assets, tilemaps and plain files are announced so the game reloads its own copies:
/// assets::reloaded(), assets::reload_count(), the AssetsReloaded event. In Release these are constants (0, false,
/// empty) and nothing reloads. See docs/api/assets.md#hot-reload.

#include <cstddef>
#include <cstdint>

#include "nnh/build.hpp"
#include "nnh/result.hpp"

namespace nnh {

/// An asset's bytes in engine memory (memory tag "assets"), released with the object. Move-only.
/// Release it before nnh::shutdown(), like any engine memory, or the leak report lists it.
class AssetData {
public:
    AssetData() = default;
    ~AssetData() { reset(); }
    AssetData(AssetData&& other) noexcept;
    AssetData& operator=(AssetData&& other) noexcept;
    AssetData(const AssetData&) = delete;
    AssetData& operator=(const AssetData&) = delete;

    const void* data() const { return data_; }
    std::size_t size() const { return size_; }
    bool valid() const { return data_ != nullptr; }
    void reset();

private:
    friend struct AssetDataAccess;
    void* data_ = nullptr;
    std::size_t size_ = 0;
};

/// A batch of reloaded assets was applied at the start of this frame (spec 031; Debug/Development only, never published
/// in Release). Delivered in the same frame, before the scene's update. Game logic must not depend on it: use it to
/// reload what the game keeps itself (a map, data structs), e.g. `if (nnh::assets::reloaded("levels/room")) ...`.
struct AssetsReloaded {
    std::uint32_t count = 0;   ///< assets::reload_count() after this batch
    std::uint16_t changed = 0; ///< paths added or replaced
    std::uint16_t removed = 0; ///< paths removed
};

namespace assets {

/// True in the builds that reload assets (Debug, Development).
inline constexpr bool kReloadEnabled = build::kDevToolsEnabled;

/// This frame's reloaded asset paths, as the index stores them ("art/hero.png"; removed paths included, index files
/// such as ".nnh_assets" too). Valid until the end of the frame.
struct ReloadedPaths {
    const char* const* paths = nullptr;
    int count = 0;
    const char* const* begin() const { return paths; }
    const char* const* end() const { return paths + count; }
};

} // namespace assets

namespace detail { // spec 031's back ends (Debug/Development only)
std::uint32_t reload_count_dev();
bool reloaded_dev(const char* path_or_name);
assets::ReloadedPaths reloaded_paths_dev();
} // namespace detail

namespace assets {

/// Applied batches since nnh::init() (+1 per batch). Release: always 0. Never allocates; any frame.
inline std::uint32_t reload_count() {
    if constexpr (kReloadEnabled) return nnh::detail::reload_count_dev();
    else return 0;
}

/// True when the batch applied this frame added, replaced or removed `path_or_name`: an exact asset path
/// ("levels/level1.nnhmap") or a load name — the path without its last extension ("levels/level1", "art/hero").
/// Release: always false. Never allocates; any frame (false on frames without a batch).
inline bool reloaded(const char* path_or_name) {
    if constexpr (kReloadEnabled) return nnh::detail::reloaded_dev(path_or_name);
    else return false;
}

/// The paths of the batch applied this frame (empty on other frames). Release: always empty. Never allocates.
inline ReloadedPaths reloaded_paths() {
    if constexpr (kReloadEnabled) return nnh::detail::reloaded_paths_dev();
    else return {};
}

/// True if the game has this asset (exact path, case included).
bool exists(const char* path);

/// Size of an asset in bytes.
Result<std::size_t> size(const char* path);

/// Reads a whole asset into `buffer`. Load time only. @return the bytes read.
/// Errors: InvalidArgument (bad path), NotFound (no such asset; a wrong letter case is logged with the
/// right one), LimitReached (`capacity` smaller than the asset), IoError, Unsupported (before init()).
Result<std::size_t> read(const char* path, void* buffer, std::size_t capacity);

/// Reads a whole asset into engine memory. Load time only. Errors as read(), plus OutOfMemory.
Result<AssetData> load(const char* path);

} // namespace assets
} // namespace nnh
