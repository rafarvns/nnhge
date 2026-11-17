// Fake platform: input (contract nnh/platform/input.hpp).

#include "fake_internal.hpp"
#include "nnh/platform/input.hpp"

namespace nnh::platform {

std::uint32_t read_buttons() { return platform_fake::detail::controls().held_buttons; }

} // namespace nnh::platform
