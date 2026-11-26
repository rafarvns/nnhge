#pragma once
// nnh::system internals shared with other core modules. Internal.

namespace nnh::detail {

/// The console remote's 3D slider (spec 026 FR-16, stereo_slider.cpp): `value` 0..1 replaces the platform's slider
/// for system::stereo_slider() (a test script sets it; 0 while a script runs until it does); a negative value
/// releases it. Never allocates.
void stereo_slider_override(float value);

/// The console remote's language (spec 029 FR-27, language_override.cpp): a code replaces the platform's for
/// system::language() (copied, at most 15 bytes; system::language() ignores one that isn't well-formed); nullptr releases
/// it. Never allocates.
void set_language_override(const char* code);

} // namespace nnh::detail
