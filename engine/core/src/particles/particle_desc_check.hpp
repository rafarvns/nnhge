#pragma once
// The description checks (spec 024 plan "Description checks"), shared by create(), check() and set_desc().

#include "nnh/particles.hpp"
#include "nnh/result.hpp"

namespace nnh::detail {

/// ErrorCode::None when `desc` is valid; otherwise InvalidArgument and a message naming the first bad field
/// ("color_keys[2].at: keys must be ascending").
Error check_particle_desc(const ParticleDesc& desc);

} // namespace nnh::detail
