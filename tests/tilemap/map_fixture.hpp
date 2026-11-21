#pragma once
// Shared helpers for the tilemap tests: maps that must be created, and the engine with test textures
// ("hero" 64x32, "tile" 16x16 from tests/gfx/test_images.cpp) for tilesets.

#include <utility>

#include "../gfx/gfx_fixture.hpp"
#include "nnh/tilemap.hpp"
#include "test.hpp"

namespace map_test {

/// Creates a map that must succeed.
inline nnh::Tilemap make(const nnh::TilemapDesc& desc) {
    auto result = nnh::Tilemap::create(desc);
    CHECK(result.ok());
    return result ? std::move(result.value()) : nnh::Tilemap{};
}

/// Adds a tileset that must succeed; returns its first id.
inline int tileset(nnh::Tilemap& map, const nnh::TilesetDesc& desc) {
    auto result = map.add_tileset(desc);
    CHECK(result.ok());
    return result ? result.value() : 0;
}

/// Adds a tile layer that must succeed; returns its index.
inline int layer(nnh::Tilemap& map, const nnh::TileLayerDesc& desc = {}) {
    auto result = map.add_tile_layer(desc);
    CHECK(result.ok());
    return result ? result.value() : -1;
}

inline nnh::Tile tile(int id, bool flip_x = false, bool flip_y = false, bool diagonal = false) {
    return {static_cast<std::uint16_t>(id), flip_x, flip_y, diagonal};
}

/// A running engine (fake platform) and a 64x32 texture: with 16x16 tiles, 8 tiles (4 per row).
struct Engine {
    nnh::Texture sheet;
    Engine() {
        gfx_test::restart();
        sheet = gfx_test::load("hero");
    }
    ~Engine() {
        sheet.reset();
        nnh::shutdown();
    }
};

} // namespace map_test
