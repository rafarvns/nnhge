#pragma once
/// @file nnh/platform/system.hpp
/// Platform contract — system description and developer requests. Implemented by every platform
/// library, called only by nnh::core. Not for games.

#include <cstdint>

#include "nnh/system.hpp"

namespace nnh::platform {

/// Capabilities of this system as a bit mask: bit (1 << Capability) set when present. Valid after init().
std::uint32_t capabilities();

/// Name, model and memory of this system. Valid after init(); strings stay valid until shutdown().
SystemInfo system_info();

/// Developer requests from system-specific controls since the last call (e.g. the PC simulator's F1).
/// Called once per frame by core. Systems without such controls return 0.
enum DevRequest : std::uint32_t {
    kDevToggleOverlay = 1u << 0,
};
std::uint32_t poll_dev_requests();

/// The stereoscopic 3D slider as the system reads it now (spec 026 FR-16): 0 (down) … 1 (up). 3DS: the hardware
/// slider; PC: the simulator's slider (F6/F7, NNH_SIM_SLIDER); systems without Capability::Stereo3D return 0. Cheap
/// (core reads it once per frame with 3D on); never allocates.
float stereo_slider();

} // namespace nnh::platform
