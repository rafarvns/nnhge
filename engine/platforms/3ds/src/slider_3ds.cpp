// Nintendo 3DS: the 3D slider (contract nnh/platform/system.hpp stereo_slider, spec 026 FR-16). libctru reads it from
// the shared memory page the system keeps up to date (no service call); 2DS models report 0. Core applies the
// Stereo3D capability and a test script's override.

#include <3ds.h>

#include "nnh/platform/system.hpp"

namespace nnh::platform {

float stereo_slider() { return osGet3DSliderState(); }

} // namespace nnh::platform
