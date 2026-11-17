// Fake platform: manual clock, moved by tests (contract nnh/platform/time.hpp).

#include "fake_internal.hpp"
#include "nnh/platform/time.hpp"

namespace nnh::platform {

std::int64_t ticks_ns() { return platform_fake::detail::controls().time_ns; }

} // namespace nnh::platform
