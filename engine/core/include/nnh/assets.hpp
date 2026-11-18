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

#include <cstddef>

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

namespace assets {

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
