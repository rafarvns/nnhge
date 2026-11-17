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
}

void set_held_buttons(std::uint32_t mask) { g_controls.held_buttons = mask; }
void set_quit_requested(bool quit) { g_controls.quit_requested = quit; }
void set_init_fails(bool fails) { g_controls.init_fails = fails; }
void set_time_ns(std::int64_t ns) { g_controls.time_ns = ns; }
void advance_time(Duration elapsed) { g_controls.time_ns += elapsed.ns; }

} // namespace nnh::platform_fake
