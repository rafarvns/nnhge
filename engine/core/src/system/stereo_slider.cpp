// The portable 3D slider (nnh/system.hpp stereo_slider, spec 026 FR-16): the platform's value, or the console remote's
// override while a test script sets it, clamped to 0..1; 0 without Capability::Stereo3D. A separate object: a game
// that never reads the slider links none of it.

#include "nnh/platform/system.hpp"
#include "nnh/system.hpp"
#include "system/system_internal.hpp"

namespace nnh {
namespace {

float g_override = -1.0f; // < 0: none

} // namespace

void detail::stereo_slider_override(float value) { g_override = value; }

float system::stereo_slider() {
    if (!has(Capability::Stereo3D)) return 0.0f;
    const float v = g_override >= 0.0f ? g_override : platform::stereo_slider();
    if (!(v > 0.0f)) return 0.0f; // NaN too
    return v < 1.0f ? v : 1.0f;
}

} // namespace nnh
