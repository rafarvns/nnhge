// Fake platform: input (contract nnh/platform/input.hpp).

#include "fake_internal.hpp"
#include "nnh/platform/input.hpp"

namespace nnh::platform {

void read_input(InputSample& out) { out = platform_fake::detail::controls().input; }

} // namespace nnh::platform
