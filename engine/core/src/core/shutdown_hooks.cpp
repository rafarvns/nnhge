// Shutdown hooks (shutdown_hooks.hpp).

#include "core/shutdown_hooks.hpp"

#include "nnh/assert.hpp"

namespace nnh::detail {

namespace {

ShutdownHook g_hooks[kMaxShutdownHooks] = {};
int g_count = 0;

} // namespace

bool add_shutdown_hook(ShutdownHook hook) {
    for (int i = 0; i < g_count; ++i) {
        if (g_hooks[i] == hook) return true;
    }
    if (g_count >= kMaxShutdownHooks) return false;
    g_hooks[g_count++] = hook;
    return true;
}

void require_shutdown_hook(ShutdownHook hook) {
    const bool added = add_shutdown_hook(hook);
    NNH_ASSERT(added, "shutdown hooks: no room (raise kMaxShutdownHooks)");
    (void)added;
}

void run_shutdown_hooks() {
    ShutdownHook waiting[kMaxShutdownHooks];
    const int count = g_count;
    for (int i = 0; i < count; ++i)
        waiting[i] = g_hooks[i];
    g_count = 0; // a hook that adds a hook again makes it wait for the next shutdown
    for (int i = count - 1; i >= 0; --i)
        waiting[i]();
}

} // namespace nnh::detail
