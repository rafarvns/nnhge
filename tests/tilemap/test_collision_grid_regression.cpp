// collision_grid() pinned before spec 019 changed the tilemap (T0.2, FR-5, FR-18, AC-3): the cells of
// collision_at and of collision_grid() (looked up directly, and through collision::move, contacts and
// cells_touching of many boxes) over loaded and code-built maps, before and after edits, hashed with FNV-1a.
// The golden values were recorded on the code before any spec 019 change; they must never change.

#include "collision/grid_internal.hpp"
#include "map_fixture.hpp"
#include "nnh/collision.hpp"
#include "tilemap/tilemap_internal.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <utility>

#ifndef NNH_TEST_MAPS_DIR
#error "NNH_TEST_MAPS_DIR is set by tests/CMakeLists.txt"
#endif

using map_test::tile;
using nnh::Rect;
using nnh::Tilemap;
using nnh::Vec2;
using nnh::collision::Cell;
namespace collision = nnh::collision;

namespace {

struct Hash {
    std::uint32_t sum = 2166136261u;
    void word(std::uint32_t w) {
        for (int i = 0; i < 4; ++i) {
            sum ^= (w >> (8 * i)) & 0xFFu;
            sum *= 16777619u;
        }
    }
    void value(float f) {
        std::uint32_t bits;
        static_assert(sizeof bits == sizeof f);
        std::memcpy(&bits, &f, sizeof bits);
        word(bits);
    }
    void sides(collision::Sides s) {
        word((s.left ? 1u : 0u) | (s.right ? 2u : 0u) | (s.ceiling ? 4u : 0u) | (s.floor ? 8u : 0u));
    }
};

std::string read_map(const char* name) {
    std::string bytes;
    const std::string path = std::string(NNH_TEST_MAPS_DIR) + "/" + name;
    std::FILE* f = std::fopen(path.c_str(), "rb");
    CHECK(f != nullptr);
    if (f == nullptr) return bytes;
    char buffer[4096];
    std::size_t got = 0;
    while ((got = std::fread(buffer, 1, sizeof buffer, f)) > 0)
        bytes.append(buffer, got);
    std::fclose(f);
    return bytes;
}

Tilemap load_converted(const char* name) {
    const std::string bytes = read_map(name);
    auto made = nnh::detail::tilemap_from_file(bytes.data(), bytes.size(), {}, name);
    CHECK(made.ok());
    return made ? std::move(made.value()) : Tilemap{};
}

/// Every cell of the map and a ring around it: collision_at and the grid's own lookup (they must agree).
std::uint32_t cells_hash(const Tilemap& map) {
    Hash h;
    const collision::Grid grid = map.collision_grid();
    for (int y = -1; y <= map.height(); ++y) {
        for (int x = -1; x <= map.width(); ++x) {
            const Cell a = map.collision_at(x, y);
            CHECK(a == collision::detail::cell_at(grid, x, y));
            h.word(static_cast<std::uint32_t>(x));
            h.word(static_cast<std::uint32_t>(y));
            h.word(static_cast<std::uint32_t>(a));
        }
    }
    return h.sum;
}

/// A fixed walk of boxes (integer hash, no rand) moved through the grid with outside Empty and Solid, plus
/// contacts and cells_touching at each end.
std::uint32_t moves_hash(const Tilemap& map) {
    Hash h;
    collision::Grid grids[2] = {map.collision_grid(), map.collision_grid()};
    grids[1].outside = Cell::Solid;
    const float world_w = static_cast<float>(map.width() * map.tile_width());
    const float world_h = static_cast<float>(map.height() * map.tile_height());
    std::uint32_t seed = 12345u;
    const auto next = [&seed] {
        seed = seed * 1664525u + 1013904223u;
        return seed >> 8;
    };
    for (int i = 0; i < 400; ++i) {
        const Rect box{static_cast<float>(next() % 4096) / 4096 * world_w - 8,
                       static_cast<float>(next() % 4096) / 4096 * world_h - 8,
                       static_cast<float>(4 + next() % 28), static_cast<float>(4 + next() % 28)};
        const Vec2 delta{static_cast<float>(static_cast<int>(next() % 513) - 256) / 4,
                         static_cast<float>(static_cast<int>(next() % 513) - 256) / 4};
        const collision::MoveOptions options{.drop_through = (i % 5) == 0};
        for (const collision::Grid& grid : grids) {
            const collision::MoveResult r = collision::move(grid, box, delta, options);
            h.value(r.position.x);
            h.value(r.position.y);
            h.value(r.moved.x);
            h.value(r.moved.y);
            h.sides(r.touching);
            const Rect end{r.position.x, r.position.y, box.w, box.h};
            h.sides(collision::contacts(grid, end));
            collision::CellHit hits[16];
            const int n = collision::cells_touching(grid, box, hits, 16);
            h.word(static_cast<std::uint32_t>(n));
            for (int k = 0; k < n && k < 16; ++k) {
                h.word(static_cast<std::uint32_t>(hits[k].x));
                h.word(static_cast<std::uint32_t>(hits[k].y));
                h.word(static_cast<std::uint32_t>(hits[k].kind));
            }
        }
    }
    return h.sum;
}

/// 20 x 12 cells; two tilesets (ids 1..8 and 9..16); a back layer with marked tiles, a `collision` ground
/// layer, a front layer with one-way and solid tiles from the second tileset, and a decoration layer without
/// collision.
struct CodeLevel {
    map_test::Engine engine;
    Tilemap map = map_test::make({.width = 20, .height = 12, .max_tile_layers = 4, .max_tile_ids = 32});
    CodeLevel() {
        map_test::tileset(map, {.texture = &engine.sheet});
        map_test::tileset(map, {.texture = &engine.sheet});
        const int back = map_test::layer(map, {.name = "back"});
        const int ground = map_test::layer(map, {.name = "ground", .collision = true});
        const int front = map_test::layer(map, {.name = "front"});
        const int deco = map_test::layer(map, {.name = "deco"});
        map.set_tile_collision(2, Cell::Solid);
        map.set_tile_collision(3, Cell::OneWay);
        map.set_tile_collision(10, Cell::OneWay);
        map.set_tile_collision(12, Cell::Solid);
        map.fill(ground, 0, 11, 20, 1, tile(1));
        map.fill(ground, 0, 0, 1, 11, tile(4));
        map.fill(back, 3, 8, 5, 1, tile(3));
        map.fill(back, 10, 6, 2, 2, tile(2));
        map.fill(front, 13, 4, 4, 1, tile(10));
        map.set_tile(front, 16, 9, tile(12));
        map.set_tile(front, 5, 8, tile(12)); // solid over a one-way plank
        map.fill(deco, 0, 0, 20, 3, tile(5));
        map.set_tile(deco, 7, 7, tile(3)); // a one-way tile on a plain layer
    }
};

void edit(Tilemap& map) {
    map.set_tile(1, 4, 11, tile(0));         // a hole in the floor
    map.fill(0, 3, 8, 2, 1, tile(1));        // part of the plank becomes plain
    map.set_layer_collision(3, true);        // the decoration layer collides
    map.set_layer_collision(1, false);       // the ground no longer does
    map.set_tile_collision(5, Cell::OneWay); // the decoration tiles become one-way
    map.set_tile_collision(12, Cell::Empty); // and the second set's solid tile goes
    map.set_tile(2, 18, 2, tile(10));
}

/// Bit per kind of cell found in the map (1 Solid, 2 OneWay): the hashes must cover both.
unsigned kinds(const Tilemap& map) {
    unsigned found = 0;
    for (int y = 0; y < map.height(); ++y) {
        for (int x = 0; x < map.width(); ++x) {
            const Cell c = map.collision_at(x, y);
            found |= c == Cell::Solid ? 1u : (c == Cell::OneWay ? 2u : 0u);
        }
    }
    return found;
}

/// True when `actual` is the golden value; prints it otherwise (to see what changed).
bool pinned(const char* what, std::uint32_t actual, std::uint32_t golden) {
    if (actual != golden) std::printf("  %s: 0x%08X, golden 0x%08X\n", what, actual, golden);
    return actual == golden;
}

} // namespace

NNH_TEST(collision_grid_regression_good_map) {
    map_test::Engine engine;
    const Tilemap map = load_converted("good.nnhmap");
    CHECK(kinds(map) == 1u); // its one-way tiles all lie under solid ones
    CHECK(pinned("cells", cells_hash(map), 0x698EB725u));
    CHECK(pinned("moves", moves_hash(map), 0xE419AD3Cu));
}

NNH_TEST(collision_grid_regression_platformer_level1) {
    map_test::Engine engine;
    const Tilemap map = load_converted("level1.nnhmap");
    CHECK(kinds(map) == 3u);
    CHECK(pinned("cells", cells_hash(map), 0x852ED0F6u));
    CHECK(pinned("moves", moves_hash(map), 0xA6D13AF0u));
}

NNH_TEST(collision_grid_regression_map_built_in_code) {
    CodeLevel l;
    CHECK(kinds(l.map) == 3u);
    CHECK(pinned("cells", cells_hash(l.map), 0x2E9EDC46u));
    CHECK(pinned("moves", moves_hash(l.map), 0x8761516Fu));
}

NNH_TEST(collision_grid_regression_map_after_edits) {
    CodeLevel l;
    edit(l.map);
    CHECK(kinds(l.map) == 3u);
    CHECK(pinned("cells", cells_hash(l.map), 0x6EA3F8C6u));
    CHECK(pinned("moves", moves_hash(l.map), 0xB2975DBAu));
}
