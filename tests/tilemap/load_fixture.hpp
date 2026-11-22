#pragma once
// Shared by the loading tests: the engine on the fake platform with a few `.nnhmap` assets and tileset images.

#include <cstring>

#include "map_file.hpp"
#include "nnh/app.hpp"
#include "nnh/memory.hpp"
#include "nnh/platform_fake.hpp"
#include "nnh/tilemap.hpp"
#include "test.hpp"
#include "tilemap/tilemap_internal.hpp"

namespace load_test {

namespace fake = nnh::platform_fake;

// The fake platform's image format: "FAKE", width, height (16-bit little endian).
inline constexpr unsigned char kWorld64x32[] = {'F', 'A', 'K', 'E', 64, 0, 32, 0};
inline constexpr unsigned char kSmall32x32[] = {'F', 'A', 'K', 'E', 32, 0, 32, 0};
inline constexpr unsigned char kTall64x64[] = {'F', 'A', 'K', 'E', 64, 0, 64, 0};

/// Files the assets point at (they must outlive the engine run).
struct Files {
    map_file::Bytes one = map_file::sample("tiles/world"); ///< levels/one
    map_file::Bytes other;                                  ///< levels/other: set by the test
};

inline Files& files() {
    static Files f;
    return f;
}

/// Restarts the engine with assets levels/one (the sample map, tileset tiles/world), levels/other (files().other)
/// and the images tiles/world (64x32), tiles/small (32x32) and tiles/tall (64x64).
inline void start(const map_file::Bytes& other = {}) {
    nnh::shutdown();
    fake::reset();
    files().one = map_file::sample("tiles/world");
    files().other = other;
    static const char index[] =
        "levels/one.nnhmap\nlevels/other.nnhmap\ntiles/small.png\ntiles/tall.png\ntiles/world.png\n";
    fake::add_asset(".nnh_assets", index, sizeof index - 1);
    fake::add_asset("levels/one.nnhmap", files().one.data(), files().one.size());
    fake::add_asset("levels/other.nnhmap", files().other.data(), files().other.size());
    fake::add_asset("tiles/world.png", kWorld64x32, sizeof kWorld64x32);
    fake::add_asset("tiles/small.png", kSmall32x32, sizeof kSmall32x32);
    fake::add_asset("tiles/tall.png", kTall64x64, sizeof kTall64x64);
    CHECK(nnh::init());
}

/// Loads a map that must load.
inline nnh::Tilemap load(const char* path, const nnh::LoadTilemapOptions& options = {}) {
    auto result = nnh::load_tilemap(path, options);
    CHECK(result.ok());
    return result ? std::move(result.value()) : nnh::Tilemap{};
}

inline nnh::ErrorCode load_error(const char* path, const nnh::LoadTilemapOptions& options = {}) {
    auto result = nnh::load_tilemap(path, options);
    return result ? nnh::ErrorCode::None : result.error().code;
}

inline std::size_t map_blocks() { return nnh::mem::tag_stats(nnh::detail::tilemap_memory_tag()).count; }
inline std::size_t map_bytes() { return nnh::mem::tag_stats(nnh::detail::tilemap_memory_tag()).bytes; }

} // namespace load_test
