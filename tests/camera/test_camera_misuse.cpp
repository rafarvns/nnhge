// Misuse (spec 018 FR-15): every case asserts in Debug/Development; Release clamps or ignores it, never
// undefined behaviour, and logs one error per rig.

#include "camera_fixture.hpp"

using nnh::CameraRig;
using nnh::CameraSettings;
using nnh::Vec2;

namespace {

CameraRig* g_rig = nullptr;
constexpr float kNan = __builtin_nanf("");
constexpr float kInf = __builtin_inff();

void zoom_too_big() {
    g_rig->set_zoom(9);
}
void zoom_too_small() {
    g_rig->set_zoom(0.1f);
}
void zoom_nan() {
    g_rig->set_zoom(kNan);
}
void negative_dead_zone() {
    g_rig->set({.dead_zone = {0, 0, -5, 10}});
}
void smoothing_zero() {
    g_rig->set({.smoothing = {0, 1}});
}
void smoothing_above_one() {
    g_rig->set({.smoothing = {1, 1.5f}});
}
void decay_zero() {
    g_rig->set({.trauma_decay = 0});
}
void speed_zero() {
    g_rig->set({.shake_speed = 0});
}
void speed_above_one() {
    g_rig->set({.shake_speed = 2});
}
void negative_gap() {
    g_rig->set({.gap = -1});
}
void nan_offset() {
    g_rig->set_offset({kNan, 0});
}
void inf_limits() {
    g_rig->set_limits({0, 0, kInf, 100});
}
void negative_limits() {
    g_rig->set_limits({0, 0, -100, 100});
}
void nan_target() {
    g_rig->update(Vec2{kNan, 0});
}
void inf_box() {
    g_rig->snap(nnh::Rect{0, kInf, 1, 1});
}
void nan_trauma() {
    g_rig->add_trauma(kNan);
}

} // namespace

NNH_TEST(every_misuse_asserts_in_debug_and_is_handled_in_release) {
    void (*const cases[])() = {zoom_too_big,    zoom_too_small,      zoom_nan,   negative_dead_zone,
                               smoothing_zero,  smoothing_above_one, decay_zero, speed_zero,
                               speed_above_one, negative_gap,        nan_offset, inf_limits,
                               negative_limits, nan_target,          inf_box,    nan_trauma};
    for (auto fn : cases) {
        CameraRig rig;
        g_rig = &rig;
        CHECK(cam_test::asserts_or_is_ignored(fn));
    }
}

NNH_TEST(release_clamps_or_keeps_the_previous_value) {
    if (nnh::build::kAssertsEnabled) return; // Debug/Development stop at the assert
    CameraRig rig;
    rig.set_zoom(9);
    CHECK(rig.settings().zoom == 4);
    rig.set_zoom(0.1f);
    CHECK(rig.settings().zoom == 0.5f);
    rig.set_zoom(kNan);
    CHECK(rig.settings().zoom == 0.5f);
    rig.set({.smoothing = {0, 7}});
    CHECK(rig.settings().smoothing.x == nnh::kCameraMinSmoothing && rig.settings().smoothing.y == 1);
    rig.set({.trauma_decay = -1, .shake_speed = 3});
    CHECK(rig.settings().trauma_decay == 1.0f / 60 && rig.settings().shake_speed == 1);
    rig.set({.gap = -4});
    CHECK(rig.settings().gap == 0);
    rig.snap(Vec2{1000, 500});
    const Vec2 before = rig.position();
    rig.update(Vec2{kNan, kNan}); // skipped
    rig.add_trauma(kNan);         // ignored
    CHECK(rig.position() == before && rig.trauma() == 0);
    rig.set_limits({0, 0, -1, 5});
    CHECK(!rig.settings().has_limits);
}

NNH_TEST(release_logs_one_error_per_rig) {
    if (nnh::build::kAssertsEnabled) return;
    cam_test::restart();
    CameraRig rig;
    const int before = nnh::platform_fake::state().console_lines;
    rig.set_zoom(9);
    CHECK(cam_test::last_log_mentions("zoom"));
    rig.update(Vec2{kNan, 0});
    rig.set_zoom(kNan);
    CHECK(nnh::platform_fake::state().console_lines == before + 1);
    CameraRig other;
    other.add_trauma(kNan);
    CHECK(nnh::platform_fake::state().console_lines == before + 2);
    nnh::shutdown();
}
