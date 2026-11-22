// The camera rig is a plain value (spec 018 FR-1): small, trivially copyable, independent copies, no init().

#include "alloc_counter.hpp"
#include "camera_fixture.hpp"
#include "nnh/memory.hpp"

#include <type_traits>

using nnh::CameraRig;
using nnh::CameraSettings;

static_assert(sizeof(CameraRig) <= 128);
static_assert(std::is_trivially_copyable_v<CameraRig>);
static_assert(std::is_trivially_copyable_v<CameraSettings>);

NNH_TEST(camera_rig_is_at_most_128_bytes_and_its_settings_84) {
    CHECK(sizeof(CameraRig) <= 128);
    CHECK(sizeof(CameraSettings) == 84);
}

NNH_TEST(camera_rig_starts_at_the_origin_with_default_settings) {
    nnh::shutdown(); // no engine needed
    const CameraRig rig;
    CHECK(cam_test::at(rig.position(), 0, 0) && cam_test::at(rig.followed(), 0, 0));
    CHECK(rig.trauma() == 0);
    CHECK(rig.settings().zoom == 1 && rig.settings().layout == nnh::CameraLayout::Single);
    CHECK(rig.settings().screen == nnh::Screen::Top &&
          rig.settings().small_world == nnh::CameraAlign::Center);
    CHECK(rig.settings().smoothing.x == 1 && rig.settings().smoothing.y == 1);
}

NNH_TEST(camera_rig_keeps_the_settings_it_is_given) {
    const CameraRig rig(
        {.small_world = nnh::CameraAlign::End, .follow_y = false, .offset = {4, -2}, .gap = 12});
    CHECK(rig.settings().small_world == nnh::CameraAlign::End && !rig.settings().follow_y);
    CHECK(cam_test::at(rig.settings().offset, 4, -2) && rig.settings().gap == 12);
}

NNH_TEST(copies_of_a_rig_move_independently) {
    CameraRig a;
    a.snap(nnh::Vec2{500, 300});
    CameraRig b = a;
    b.snap(nnh::Vec2{1000, 1000});
    CHECK(cam_test::at(a.position(), 300, 180));
    CHECK(cam_test::at(b.position(), 800, 880));
}

NNH_TEST(a_rig_allocates_nothing_while_updating_applying_and_converting) {
    cam_test::restart();
    CameraRig rig({.layout = nnh::CameraLayout::TallWorld,
                   .dead_zone = CameraSettings::centered_zone(40, 30),
                   .smoothing = {0.3f, 0.2f}});
    rig.set_limits({0, 0, 2000, 1000});
    const auto news = alloc_counter::news();
    const std::size_t engine = nnh::mem::live_allocations();
    CHECK(nnh::frame());
    for (int i = 0; i < 1000; ++i) {
        if (i % 100 == 0) rig.add_trauma(0.5f);
        rig.update(nnh::Vec2{static_cast<float>(i), static_cast<float>(i % 300)});
        rig.apply();
        rig.debug_draw();
        (void)rig.to_world(nnh::Screen::Bottom, rig.to_screen(nnh::Screen::Top, {5, 6}));
        if (i % 50 == 0) CHECK(nnh::frame());
    }
    CHECK(alloc_counter::news() == news && nnh::mem::live_allocations() == engine);
    nnh::shutdown();
}

NNH_TEST(centered_zone_is_a_rect_around_the_view_centre) {
    constexpr nnh::Rect z = CameraSettings::centered_zone(64, 48);
    CHECK(z.x == -32 && z.y == -24 && z.w == 64 && z.h == 48);
}
