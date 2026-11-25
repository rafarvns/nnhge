#pragma once
/// @file nnh/system.hpp
/// What the running system offers. Use capability queries instead of platform #ifdef.
/// See docs/api/system.md and docs/architecture/platforms.md.
///
/// @code
/// if (nnh::system::has(nnh::Capability::Stereo3D)) enable_depth_effects();
/// @endcode

#include <cstdint>

namespace nnh {

/// Hardware features. A capability describes the system; engine APIs for some of them (touch, Circle
/// Pad, stereo 3D) arrive in later milestones.
enum class Capability : std::uint8_t {
    SecondScreen, ///< a second screen (3DS bottom screen; simulated on PC)
    TouchScreen,  ///< touch input on the second screen
    Stereo3D,     ///< stereoscopic top screen with a depth slider
    CirclePad,    ///< analog stick
    CStick,       ///< second analog stick (New 3DS)
    ZButtons,     ///< ZL/ZR (New 3DS)
};
inline constexpr int kCapabilityCount = 6;

struct SystemInfo {
    const char* name;        ///< "Nintendo 3DS", "PC simulator"
    const char* model;       ///< "New 3DS", "Old 3DS", "Windows"...
    std::uint32_t memory_mb; ///< memory available to the game, approximate
};

namespace system {

bool has(Capability capability);
SystemInfo info();
const char* capability_name(Capability capability);

/// The 3D slider (spec 026 FR-16): 0 (down, or no Capability::Stereo3D) … 1 (up), read now. 3DS: the hardware slider;
/// the PC simulator: its own slider (F6/F7, NNH_SIM_SLIDER). While a test script runs (`nnh run --script`, Debug and
/// Development builds) the script's `slider` command sets it instead, 0 until it does. Cheap; never allocates.
/// 3D drawing reads it for stereo (docs/api/gfx3d.md#stereo); a 2D game may read it for its own effects.
float stereo_slider();

} // namespace system
} // namespace nnh
