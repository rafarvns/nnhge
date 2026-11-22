// Tilemap::bounds() (spec 018 FR-7): the map's world rectangle from (0, 0), for CameraRig::set_limits.

#include "load_fixture.hpp"
#include "map_fixture.hpp"

NNH_TEST(bounds_of_a_code_built_map_are_its_size_in_world_pixels) {
    map_test::Engine engine;
    const nnh::Tilemap map = map_test::make({.width = 30, .height = 20, .tile_width = 16, .tile_height = 8});
    const nnh::Rect r = map.bounds();
    CHECK(r.x == 0 && r.y == 0 && r.w == 480 && r.h == 160);
}

NNH_TEST(bounds_of_a_loaded_map_are_its_size_in_world_pixels) {
    load_test::start();
    {
        const nnh::Tilemap map = load_test::load("levels/one"); // 8 × 4 cells of 16 × 16
        const nnh::Rect r = map.bounds();
        CHECK(r.x == 0 && r.y == 0 && r.w == 128 && r.h == 64);
    }
    nnh::shutdown();
}

NNH_TEST(bounds_of_an_empty_map_are_empty) {
    const nnh::Tilemap empty;
    const nnh::Rect r = empty.bounds();
    CHECK(r.x == 0 && r.y == 0 && r.w == 0 && r.h == 0);
}
