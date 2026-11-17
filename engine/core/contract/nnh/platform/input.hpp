#pragma once
/// @file nnh/platform/input.hpp
/// Platform contract — input. Implemented by every platform library, called only by nnh::core.
/// Not for games. See docs/architecture/platforms.md.

#include <cstdint>

namespace nnh::platform {

/// Buttons currently held, as a mask of nnh::Button bits (<nnh/input.hpp>). Called once per frame by
/// nnh::frame(), after poll_events(). Returns 0 when nothing is held. Bits for buttons the system doesn't
/// have (e.g. ZL on Old 3DS) are never set. Must not allocate.
std::uint32_t read_buttons();

} // namespace nnh::platform
