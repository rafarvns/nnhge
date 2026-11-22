// update() advances exactly one 60 Hz frame (spec 018 FR-12): never by wall time; not calling it freezes the
// rig.

#include "camera_fixture.hpp"
#include "nnh/profile.hpp"

using cam_test::same_bits;
using nnh::CameraRig;
using nnh::Vec2;

NNH_TEST(update_moves_one_frame_per_call_whatever_the_clock_says) {
    cam_test::restart();
    CameraRig a({.smoothing = {0.2f, 0.2f}});
    CameraRig b = a;
    a.add_trauma(0.8f);
    b.add_trauma(0.8f);
    for (int i = 0; i < 30; ++i) {
        a.update(Vec2{500, 300});
        nnh::platform_fake::advance_time(nnh::Duration::from_seconds(1)); // a second between b's frames
        CHECK(nnh::frame());
        b.update(Vec2{500, 300});
        CHECK(same_bits(a.position(), b.position()) && a.trauma() == b.trauma());
    }
    nnh::shutdown();
}

NNH_TEST(not_calling_update_freezes_the_camera) {
    CameraRig rig({.smoothing = {0.2f, 0.2f}});
    rig.add_trauma(1);
    rig.update(Vec2{500, 300});
    const Vec2 p = rig.position();
    const float t = rig.trauma();
    (void)rig.camera(nnh::Screen::Top); // queries don't advance it
    (void)rig.view(nnh::Screen::Top);
    CHECK(same_bits(rig.position(), p) && rig.trauma() == t);
}

NNH_TEST(update_is_a_profile_scope_named_camera) {
    if (!nnh::build::kDevToolsEnabled) return; // the profiler is compiled out of Release
    cam_test::restart();
    CameraRig rig;
    CHECK(nnh::frame());
    for (int i = 0; i < 3; ++i)
        rig.update(Vec2{1, 2});
    CHECK(nnh::frame());
    nnh::profile::ScopeStats stats[nnh::profile::kMaxScopes];
    const int n = nnh::profile::slowest(stats, nnh::profile::kMaxScopes);
    int calls = 0;
    for (int i = 0; i < n; ++i) {
        if (std::strcmp(stats[i].name, "camera") == 0) calls = stats[i].calls;
    }
    CHECK(calls == 3);
    nnh::shutdown();
}

NNH_TEST(a_rig_never_snapped_moves_from_the_origin) {
    CameraRig rig({.smoothing = {0.5f, 0.5f}});
    rig.update(Vec2{600, 320}); // desired (400, 200)
    CHECK(cam_test::at(rig.position(), 200, 100));
}
