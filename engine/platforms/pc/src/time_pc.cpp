// PC platform: monotonic clock (contract nnh/platform/time.hpp).

#include <SDL3/SDL.h>

#include "nnh/platform/time.hpp"

namespace nnh::platform {

std::int64_t ticks_ns() { return static_cast<std::int64_t>(SDL_GetTicksNS()); }

} // namespace nnh::platform
