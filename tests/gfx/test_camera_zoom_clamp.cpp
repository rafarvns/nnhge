// Camera2D::zoom (spec 018 FR-9, decision D33): 1 by default, set_camera keeps it in [0.5, 4] (asserting in
// Debug/Development), NaN becomes 1.

#include "../camera/camera_fixture.hpp"
#include "gfx_fixture.hpp"

using nnh::Camera2D;
using nnh::Screen;
namespace gfx = nnh::gfx;

namespace {

void set_zoom(float zoom) { gfx::set_camera(Screen::Top, {{3, 4}, zoom}); }
void too_big() { set_zoom(9); }
void too_small() { set_zoom(0.25f); }
void nan_zoom() { set_zoom(__builtin_nanf("")); }

} // namespace

NNH_TEST(camera_zoom_is_1_by_default_and_after_init) {
    CHECK(Camera2D{}.zoom == 1);
    gfx_test::restart();
    CHECK(gfx::camera(Screen::Top).zoom == 1 && gfx::camera(Screen::Bottom).zoom == 1);
    nnh::shutdown();
}

NNH_TEST(set_camera_keeps_a_zoom_in_range) {
    gfx_test::restart();
    const float zooms[] = {nnh::kMinCameraZoom, 0.75f, 1, 2, 3.5f, nnh::kMaxCameraZoom};
    for (float z : zooms) {
        gfx::set_camera(Screen::Bottom, {{1, 2}, z});
        CHECK(gfx::camera(Screen::Bottom).zoom == z);
    }
    nnh::shutdown();
}

NNH_TEST(set_camera_with_a_zoom_out_of_range_asserts_or_clamps) {
    gfx_test::restart();
    CHECK(cam_test::asserts_or_is_ignored(too_big));
    CHECK(cam_test::asserts_or_is_ignored(too_small));
    CHECK(cam_test::asserts_or_is_ignored(nan_zoom));
    if (!nnh::build::kAssertsEnabled) {
        too_big();
        CHECK(gfx::camera(Screen::Top).zoom == 4);
        too_small();
        CHECK(gfx::camera(Screen::Top).zoom == 0.5f);
        nan_zoom();
        CHECK(gfx::camera(Screen::Top).zoom == 1);
        CHECK(gfx::camera(Screen::Top).position == (nnh::Vec2{3, 4}));
    }
    nnh::shutdown();
}
