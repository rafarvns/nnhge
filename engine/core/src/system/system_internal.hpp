#pragma once
// nnh::system internals shared with other core modules. Internal.

namespace nnh::detail {

/// The console remote's 3D slider (spec 026 FR-16, stereo_slider.cpp): `value` 0..1 replaces the platform's slider
/// for system::stereo_slider() (a test script sets it; 0 while a script runs until it does); a negative value
/// releases it. Never allocates.
void stereo_slider_override(float value);

} // namespace nnh::detail
