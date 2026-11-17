// Fake platform: application lifecycle (contract nnh/platform/app.hpp).

#include "fake_internal.hpp"
#include "nnh/platform/app.hpp"

namespace nnh::platform {

using platform_fake::detail::controls;
using platform_fake::detail::mutable_state;

bool init(const AppConfig&) {
    ++mutable_state().init_calls;
    if (controls().init_fails) return false;
    mutable_state().initialized = true;
    return true;
}

void shutdown() {
    ++mutable_state().shutdown_calls;
    mutable_state().initialized = false;
}

bool poll_events() { return !controls().quit_requested; }

} // namespace nnh::platform
