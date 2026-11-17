// nnh::system — capability queries and system info (nnh/system.hpp).

#include "nnh/system.hpp"

#include "nnh/platform/system.hpp"

namespace nnh::system {

bool has(Capability capability) {
    return (platform::capabilities() & (1u << static_cast<unsigned>(capability))) != 0;
}

SystemInfo info() { return platform::system_info(); }

const char* capability_name(Capability capability) {
    switch (capability) {
    case Capability::SecondScreen: return "SecondScreen";
    case Capability::TouchScreen: return "TouchScreen";
    case Capability::Stereo3D: return "Stereo3D";
    case Capability::CirclePad: return "CirclePad";
    case Capability::CStick: return "CStick";
    case Capability::ZButtons: return "ZButtons";
    }
    return "?";
}

} // namespace nnh::system
