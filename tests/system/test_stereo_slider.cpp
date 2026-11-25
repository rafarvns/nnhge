// The portable 3D slider (spec 026 FR-16, T11.1): the platform's value with Capability::Stereo3D, 0 without it,
// clamped; a test script's override (the console remote) replaces the platform's value until it is released.

#include <limits>

#include "nnh/app.hpp"
#include "nnh/build.hpp"
#include "nnh/platform_fake.hpp"
#include "nnh/system.hpp"
#include "system/system_internal.hpp"
#include "test.hpp"

namespace fake = nnh::platform_fake;
using nnh::Capability;

namespace {

void start(bool stereo) {
    nnh::shutdown();
    fake::reset();
    if (stereo) fake::set_capabilities({Capability::SecondScreen, Capability::Stereo3D});
    CHECK(nnh::init());
}

} // namespace

NNH_TEST(stereo_slider_is_zero_without_the_capability) {
    start(false);
    fake::set_stereo_slider(0.5f);
    CHECK(nnh::system::stereo_slider() == 0.0f);
    nnh::shutdown();
}

NNH_TEST(stereo_slider_reads_the_platform_with_the_capability) {
    start(true);
    CHECK(nnh::system::stereo_slider() == 0.0f); // the fake's slider starts down
    fake::set_stereo_slider(0.25f);
    CHECK(nnh::system::stereo_slider() == 0.25f);
    nnh::shutdown();
}

NNH_TEST(stereo_slider_is_clamped_to_0_1) {
    start(true);
    fake::set_stereo_slider(1.5f);
    CHECK(nnh::system::stereo_slider() == 1.0f);
    fake::set_stereo_slider(-0.5f);
    CHECK(nnh::system::stereo_slider() == 0.0f);
    fake::set_stereo_slider(std::numeric_limits<float>::quiet_NaN());
    CHECK(nnh::system::stereo_slider() == 0.0f);
    nnh::shutdown();
}

NNH_TEST(stereo_slider_override_replaces_the_platform_until_released) {
    start(true);
    fake::set_stereo_slider(0.75f);
    nnh::detail::stereo_slider_override(0.0f); // a script attached: 0 until it sets one
    CHECK(nnh::system::stereo_slider() == 0.0f);
    nnh::detail::stereo_slider_override(0.5f);
    CHECK(nnh::system::stereo_slider() == 0.5f);
    nnh::detail::stereo_slider_override(-1.0f); // released
    CHECK(nnh::system::stereo_slider() == 0.75f);
    nnh::shutdown();
}

NNH_TEST(stereo_slider_override_needs_the_capability_too) {
    start(false);
    nnh::detail::stereo_slider_override(1.0f);
    CHECK(nnh::system::stereo_slider() == 0.0f);
    nnh::detail::stereo_slider_override(-1.0f);
    nnh::shutdown();
}

NNH_TEST(stereo_slider_override_is_forgotten_by_init) {
    if constexpr (!nnh::build::kDevToolsEnabled) return; // Release has no console remote to set it
    start(true);
    nnh::detail::stereo_slider_override(1.0f);
    start(true);
    CHECK(nnh::system::stereo_slider() == 0.0f);
    nnh::shutdown();
}
