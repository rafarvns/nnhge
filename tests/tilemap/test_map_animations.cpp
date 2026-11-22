// Animated tiles on a map (spec 012 stage 6, FR-0, FR-2, FR-10): set_tile_animation, its limits and errors, the
// frame shown at a time, a map built in code with animations is a valid file, and drawing shows the current frame.

#include <cstring>

#include "map_file.hpp"
#include "map_fixture.hpp"
#include "nnh/platform/video.hpp"
#include "tilemap/map_validate.hpp"

namespace fake = nnh::platform_fake;
using map_test::layer;
using map_test::make;
using map_test::tile;
using map_test::tileset;
using nnh::Duration;
using nnh::ErrorCode;
using nnh::Tilemap;
using Frame = nnh::Tilemap::AnimationFrame;

namespace {

/// The engine, a map of 8 x 4 cells with "hero" (ids 1..8) and "tile" (id 9) as tilesets and room for
/// `animations` animations of `frames` frames in all.
struct Animated {
    map_test::Engine engine;
    nnh::Texture single;
    Tilemap map;

    explicit Animated(int animations = 4, int frames = 8) {
        single = gfx_test::load("tile");
        map = make({.width = 8, .height = 4, .max_animations = animations, .max_animation_frames = frames});
        CHECK(tileset(map, {.texture = &engine.sheet}) == 1);
        CHECK(tileset(map, {.texture = &single}) == 9);
    }
    ~Animated() {
        map.reset();
        single.reset();
    }
    ErrorCode set(int tile_id, const Frame* frames, int count) {
        auto r = map.set_tile_animation(tile_id, frames, count);
        return r ? ErrorCode::None : r.error().code;
    }
    int frames_of(int tile_id) const { return map.tile_animation(tile_id, nullptr, 0); }
    int shown(int tile_id, int ms) const { return map.animated_tile_id(tile_id, Duration::from_ms(ms)); }
};

constexpr Frame kWater[] = {{.tile_id = 1, .ms = 100}, {.tile_id = 2, .ms = 50}};

} // namespace

NNH_TEST(set_tile_animation_animates_a_tile) {
    Animated a;
    CHECK(a.set(1, kWater, 2) == ErrorCode::None);
    Frame out[4] = {};
    CHECK(a.map.tile_animation(1, out, 4) == 2);
    CHECK(out[0].tile_id == 1 && out[0].ms == 100 && out[1].tile_id == 2 && out[1].ms == 50);
    CHECK(a.shown(1, 0) == 1 && a.shown(1, 99) == 1 && a.shown(1, 100) == 2 && a.shown(1, 150) == 1);
    CHECK(a.frames_of(2) == 0 && a.shown(2, 100) == 2); // not animated: itself
}

NNH_TEST(tile_animation_copies_at_most_its_capacity) {
    Animated a;
    CHECK(a.set(1, kWater, 2) == ErrorCode::None);
    Frame out[2] = {};
    CHECK(a.map.tile_animation(1, out, 1) == 2);
    CHECK(out[0].tile_id == 1 && out[1].tile_id == 0);
    CHECK(a.map.tile_animation(0, out, 2) == 0 && a.map.tile_animation(99, out, 2) == 0);
}

NNH_TEST(an_animation_shows_frame_zero_before_time_zero) {
    Animated a;
    CHECK(a.set(1, kWater, 2) == ErrorCode::None);
    CHECK(a.shown(1, -120) == 1 && a.shown(1, -50) == 1 && a.shown(1, -200) == 1);
}

NNH_TEST(animations_of_several_tiles_in_any_order_keep_their_own_frames) {
    Animated a;
    const Frame five[] = {{.tile_id = 6, .ms = 10}};
    const Frame two[] = {{.tile_id = 3, .ms = 20}, {.tile_id = 4, .ms = 20}, {.tile_id = 5, .ms = 20}};
    const Frame seven[] = {{.tile_id = 8, .ms = 30}, {.tile_id = 7, .ms = 30}};
    CHECK(a.set(5, five, 1) == ErrorCode::None);
    CHECK(a.set(2, two, 3) == ErrorCode::None);
    CHECK(a.set(7, seven, 2) == ErrorCode::None);
    CHECK(a.frames_of(5) == 1 && a.frames_of(2) == 3 && a.frames_of(7) == 2);
    CHECK(a.shown(5, 0) == 6);
    CHECK(a.shown(2, 0) == 3 && a.shown(2, 25) == 4 && a.shown(2, 45) == 5);
    CHECK(a.shown(7, 0) == 8 && a.shown(7, 30) == 7);
}

NNH_TEST(replacing_an_animation_changes_only_its_frames) {
    Animated a(4, 5);
    const Frame three[] = {{.tile_id = 3, .ms = 20}, {.tile_id = 4, .ms = 20}, {.tile_id = 5, .ms = 20}};
    const Frame later[] = {{.tile_id = 8, .ms = 30}};
    CHECK(a.set(2, three, 3) == ErrorCode::None);
    CHECK(a.set(7, later, 1) == ErrorCode::None);
    CHECK(a.set(2, kWater, 2) == ErrorCode::None); // 3 frames → 2
    CHECK(a.frames_of(2) == 2 && a.shown(2, 100) == 2);
    CHECK(a.frames_of(7) == 1 && a.shown(7, 0) == 8);
    CHECK(a.set(1, kWater, 2) == ErrorCode::None); // the freed frame is room again: 2 + 1 + 2 = 5
    CHECK(a.set(2, three, 3) == ErrorCode::LimitReached);
    CHECK(a.frames_of(2) == 2 && a.frames_of(1) == 2 && a.shown(7, 0) == 8); // unchanged by the refusal
}

NNH_TEST(count_zero_removes_an_animation_and_frees_its_room) {
    Animated a(1, 2);
    CHECK(a.set(1, kWater, 2) == ErrorCode::None);
    CHECK(a.set(1, nullptr, 0) == ErrorCode::None);
    CHECK(a.frames_of(1) == 0 && a.shown(1, 100) == 1);
    map_file::Bytes file = map_file::from_map(a.map); // the tile's animated flag went with it
    CHECK(nnh::detail::validate_map(file.data(), file.size()).ok());
    CHECK(a.set(3, nullptr, 0) == ErrorCode::None); // removing a missing one is fine
    CHECK(a.set(4, kWater, 2) == ErrorCode::None);   // the one animation slot and both frames are free
}

NNH_TEST(set_tile_animation_refuses_bad_arguments) {
    Animated a;
    const Frame other_set[] = {{.tile_id = 9, .ms = 10}};
    const Frame zero_ms[] = {{.tile_id = 2, .ms = 0}};
    const Frame unknown[] = {{.tile_id = 10, .ms = 10}};
    const Frame empty[] = {{.tile_id = 0, .ms = 10}};
    CHECK(a.set(0, kWater, 2) == ErrorCode::InvalidArgument);
    CHECK(a.set(10, kWater, 2) == ErrorCode::InvalidArgument);
    CHECK(a.set(1, other_set, 1) == ErrorCode::InvalidArgument);
    CHECK(a.set(9, kWater, 1) == ErrorCode::InvalidArgument); // tile 9's set has only tile 9
    CHECK(a.set(1, zero_ms, 1) == ErrorCode::InvalidArgument);
    CHECK(a.set(1, unknown, 1) == ErrorCode::InvalidArgument);
    CHECK(a.set(1, empty, 1) == ErrorCode::InvalidArgument);
    CHECK(a.set(1, nullptr, 1) == ErrorCode::InvalidArgument);
    CHECK(a.set(1, kWater, -1) == ErrorCode::InvalidArgument);
    CHECK(a.frames_of(1) == 0);
    Tilemap none;
    CHECK(!none.set_tile_animation(1, kWater, 2).ok());
    CHECK(none.tile_animation(1, nullptr, 0) == 0 && none.animated_tile_id(1, {}) == 1);
}

NNH_TEST(set_tile_animation_refuses_more_than_its_room) {
    Animated a(2, 40);
    Frame many[33];
    for (Frame& f : many) f = {.tile_id = 3, .ms = 5};
    CHECK(a.set(1, many, 33) == ErrorCode::LimitReached); // over 32 frames in one animation
    CHECK(a.set(1, many, 32) == ErrorCode::None);
    CHECK(a.set(2, many, 9) == ErrorCode::LimitReached); // 41 frames in all
    CHECK(a.set(2, many, 8) == ErrorCode::None);
    CHECK(a.set(3, many, 0) == ErrorCode::None);
    const Frame one[] = {{.tile_id = 3, .ms = 5}};
    CHECK(a.set(1, one, 1) == ErrorCode::None);
    CHECK(a.set(3, one, 1) == ErrorCode::LimitReached); // a third animation
    Animated by_default(0, 0);
    CHECK(by_default.set(1, kWater, 2) == ErrorCode::LimitReached);
}

NNH_TEST(an_animated_tile_is_still_itself_to_queries_and_collision) {
    Animated a;
    a.map.set_tile_collision(1, nnh::collision::Cell::Solid);
    const int l = layer(a.map);
    a.map.set_tile(l, 2, 1, tile(1));
    CHECK(a.set(1, kWater, 2) == ErrorCode::None);
    CHECK(a.map.tile(l, 2, 1).id == 1);
    CHECK(a.map.collision_at(2, 1) == nnh::collision::Cell::Solid);
    CHECK(a.map.tile_collision(2) == nnh::collision::Cell::Empty);
}

NNH_TEST(a_map_built_with_animations_is_a_valid_map_file) {
    Animated a;
    const Frame two[] = {{.tile_id = 3, .ms = 20}, {.tile_id = 4, .ms = 20}, {.tile_id = 5, .ms = 20}};
    CHECK(a.set(5, kWater, 2) == ErrorCode::None);
    CHECK(a.set(2, two, 3) == ErrorCode::None);
    CHECK(a.set(5, two, 1) == ErrorCode::None);
    const Frame nine[] = {{.tile_id = 9, .ms = 70}};
    CHECK(a.set(9, nine, 1) == ErrorCode::None);
    map_file::Bytes file = map_file::from_map(a.map);
    CHECK(nnh::detail::validate_map(file.data(), file.size()).ok());
}

// --- drawing ---

namespace {

/// The engine with max_tiles and a map whose tile 1 shows tiles 1 (100 ms) and 2 (100 ms), placed at (0, 0).
struct Drawn {
    nnh::Texture sheet;
    Tilemap map;
    Drawn() {
        gfx_test::restart({.gfx = {.max_tiles = 64}});
        sheet = gfx_test::load("hero");
        map = make({.width = 4, .height = 2, .max_animations = 1, .max_animation_frames = 2});
        CHECK(tileset(map, {.texture = &sheet}) == 1);
        const int l = layer(map);
        map.set_tile(l, 0, 0, tile(1));
        map.set_tile(l, 1, 0, tile(3));
        const Frame frames[] = {{.tile_id = 1, .ms = 100}, {.tile_id = 2, .ms = 100}};
        CHECK(map.set_tile_animation(1, frames, 2).ok());
    }
    ~Drawn() {
        map.reset();
        sheet.reset();
        nnh::shutdown();
    }
    /// Draws one frame and returns the source x of the quads at (0, 0) and (16, 0).
    int draw(nnh::gfx::TilemapDrawOptions options = {}) {
        CHECK(nnh::frame());
        nnh::gfx::draw_tilemap(nnh::Screen::Top, map, options);
        CHECK(nnh::frame());
        const fake::FakeTiles& t = fake::state().last_frame_tiles;
        CHECK(t.quad_count == 2 && t.quads[1].src_x == 32); // tile 3 never changes
        return t.quad_count == 2 ? t.quads[0].src_x : -1;
    }
};

} // namespace

NNH_TEST(drawing_shows_the_frame_of_the_given_animation_time) {
    Drawn d;
    CHECK(d.draw({.animation_time = Duration::from_ms(0)}) == 0);    // tile 1
    CHECK(d.draw({.animation_time = Duration::from_ms(150)}) == 16); // tile 2
    CHECK(d.draw({.animation_time = Duration::from_ms(250)}) == 0);
}

NNH_TEST(drawing_follows_the_frame_count_by_default) {
    Drawn d;
    int changes = 0, last = -1;
    for (int i = 0; i < 20; ++i) {
        CHECK(nnh::frame());
        const std::uint64_t frame = nnh::frame_count();
        nnh::gfx::draw_tilemap(nnh::Screen::Top, d.map);
        CHECK(nnh::frame());
        const int want = frame * 1000 / 60 % 200 < 100 ? 0 : 16;
        CHECK(fake::state().last_frame_tiles.quads[0].src_x == want);
        changes += last >= 0 && want != last ? 1 : 0;
        last = want;
    }
    CHECK(changes >= 2); // 40 frames ≈ 667 ms: the animation turned over several times
}

NNH_TEST(an_explicit_animation_time_of_zero_is_not_the_default_clock) {
    Drawn d;
    // Until the default clock of the next frame (the one draw() records in) shows tile 2.
    while ((nnh::frame_count() + 1) * 1000 / 60 % 200 < 100) CHECK(nnh::frame());
    CHECK(d.draw({.animation_time = Duration::from_ms(0)}) == 0);
}

NNH_TEST(drawing_animates_tiles_of_a_second_tileset) {
    gfx_test::restart({.gfx = {.max_tiles = 64}});
    {
        nnh::Texture sheet = gfx_test::load("hero");
        Tilemap map = make({.width = 4, .height = 2, .max_animations = 1, .max_animation_frames = 2});
        CHECK(tileset(map, {.texture = &sheet}) == 1);
        CHECK(tileset(map, {.texture = &sheet}) == 9); // the same sheet again: ids 9..16
        map.set_tile(layer(map), 2, 0, tile(9));
        const Frame frames[] = {{.tile_id = 9, .ms = 100}, {.tile_id = 10, .ms = 100}};
        CHECK(map.set_tile_animation(9, frames, 2).ok());
        CHECK(nnh::frame());
        nnh::gfx::draw_tilemap(nnh::Screen::Top, map, {.animation_time = Duration::from_ms(150)});
        CHECK(nnh::frame());
        const fake::FakeTiles& t = fake::state().last_frame_tiles;
        CHECK(t.quad_count == 1 && t.quads[0].src_x == 16); // tile 10
        map.reset();
    }
    nnh::shutdown();
}
