// PC platform: system description (contract nnh/platform/system.hpp).

#include <SDL3/SDL.h>

#include "context_pc.hpp"
#include "nnh/platform/system.hpp"

namespace nnh::platform {

std::uint32_t capabilities() {
    // What the simulator offers today: both screens. Touch (mouse) and analog input come in M1b.
    return 1u << static_cast<unsigned>(Capability::SecondScreen);
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

} // namespace nnh::platform
