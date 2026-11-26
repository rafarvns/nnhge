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
    kDevTogglePause = 1u << 1, ///< pause or resume the game (the simulator's Pause key; spec 030 D52)
    kDevStepFrame = 1u << 2,   ///< run one game frame, then pause (the simulator's F10)
    kDevHoldAtStart = 1u << 3, ///< returned once: start paused before frame 1 (NNH_HOLD=1, 3DS argv --nnh-hold)
};
std::uint32_t poll_dev_requests();

/// The stereoscopic 3D slider as the system reads it now (spec 026 FR-16): 0 (down) … 1 (up). 3DS: the hardware
/// slider; PC: the simulator's slider (F6/F7, NNH_SIM_SLIDER); systems without Capability::Stereo3D return 0. Cheap
/// (core reads it once per frame with 3D on); never allocates.
float stereo_slider();

/// The system's language as a well-formed BCP 47-style code (spec 029 FR-27; at most 15 bytes, never null): 3DS: the
/// console's setting and region (CFG); PC: NNH_SIM_LANGUAGE, else language.ini, else "en"; fake: set by tests. Cheap
/// after the first call (cached); never allocates.
const char* system_language();

} // namespace nnh::platform
