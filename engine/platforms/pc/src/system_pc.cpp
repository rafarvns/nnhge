// PC platform: system description (contract nnh/platform/system.hpp).

#include <SDL3/SDL.h>

#include "context_pc.hpp"
#include "nnh/platform/system.hpp"

namespace nnh::platform {

std::uint32_t capabilities() {
    // The simulator offers every 3DS control (keyboard, mouse and gamepad stand in for them) and a 3D slider (F6/F7,
    // NNH_SIM_SLIDER) whose eyes it shows one at a time or as an anaglyph (F8, spec 026 FR-16/FR-22).
    constexpr Capability kSimulated[] = {Capability::SecondScreen, Capability::TouchScreen, Capability::Stereo3D,
                                         Capability::CirclePad,    Capability::CStick,      Capability::ZButtons};
    std::uint32_t caps = 0;
    for (const Capability c : kSimulated) caps |= 1u << static_cast<unsigned>(c);
    return caps;
}

SystemInfo system_info() {
    return SystemInfo{"PC simulator", SDL_GetPlatform(), static_cast<std::uint32_t>(SDL_GetSystemRAM())};
}

std::uint32_t poll_dev_requests() {
    pc::Context& ctx = pc::context();
    const std::uint32_t requests = ctx.dev_requests;
    ctx.dev_requests = 0;
    return requests;
}

float stereo_slider() { return pc::context().slider; }

} // namespace nnh::platform
