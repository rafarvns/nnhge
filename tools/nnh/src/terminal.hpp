#pragma once
// The terminal nnh writes to (spec 010 FR-5).

namespace nnh::tool {

/// True if stdout is a terminal that shows ANSI colours (turning them on in the Windows console) and
/// NO_COLOR isn't set.
bool enable_colour();

} // namespace nnh::tool
