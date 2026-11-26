// Fake platform: application lifecycle (contract nnh/platform/app.hpp).

#include "fake_internal.hpp"
#include "nnh/platform/app.hpp"

namespace {

const char* g_last_error = "";

} // namespace

void nnh::platform_fake::detail::set_last_error(const char* message) { g_last_error = message; }

namespace nnh::platform {

using platform_fake::detail::controls;
using platform_fake::detail::mutable_state;

bool init(const AppConfig&) {
    ++mutable_state().init_calls;
    if (controls().init_fails) {
        g_last_error = "fake init failure (set_init_fails)";
        return false;
    }
    g_last_error = "";
    mutable_state().initialized = true;
    return true;
}

void shutdown() {
    ++mutable_state().shutdown_calls;
    mutable_state().initialized = false;
}

const char* last_error() { return g_last_error; }

bool poll_events() {
    platform_fake::detail::Controls& c = controls();
    if (c.quit_after_polls == 0) c.quit_requested = true;
    if (c.quit_after_polls > 0) --c.quit_after_polls;
    return !c.quit_requested;
}

} // namespace nnh::platform
