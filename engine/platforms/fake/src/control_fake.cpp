// Fake platform: test controls and recordings (nnh/platform_fake.hpp).

#include "fake_internal.hpp"

namespace nnh::platform_fake {
namespace {

State g_state;
detail::Controls g_controls;

} // namespace

State& detail::mutable_state() { return g_state; }
detail::Controls& detail::controls() { return g_controls; }

const State& state() { return g_state; }

void reset() {
    g_state = {};
    g_controls = {};
    detail::fs_reset();
    detail::remote_reset();
    detail::video3d_reset();
    detail::alpha_reset();
    detail::language_reset();
}

void set_held_buttons(std::uint32_t mask) { g_controls.input.buttons = mask; }
void set_circle_pad(float x, float y) {
    g_controls.input.circle_x = x;
    g_controls.input.circle_y = y;
}
void set_c_stick(float x, float y) {
    g_controls.input.cstick_x = x;
    g_controls.input.cstick_y = y;
}
void set_touch(int x, int y) {
    g_controls.input.touching = true;
    g_controls.input.touch_x = static_cast<std::int16_t>(x);
    g_controls.input.touch_y = static_cast<std::int16_t>(y);
}
void release_touch() { g_controls.input.touching = false; }
void add_asset(const char* path, const void* data, std::size_t size) {
    if (g_controls.asset_count < detail::kMaxFakeAssets) g_controls.assets[g_controls.asset_count++] = {path, data, size};
}
void set_quit_requested(bool quit) { g_controls.quit_requested = quit; }
void set_init_fails(bool fails) { g_controls.init_fails = fails; }
void set_time_ns(std::int64_t ns) { g_controls.time_ns = ns; }
void advance_time(Duration elapsed) { g_controls.time_ns += elapsed.ns; }

} // namespace nnh::platform_fake
