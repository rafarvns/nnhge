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

} // namespace system
} // namespace nnh
