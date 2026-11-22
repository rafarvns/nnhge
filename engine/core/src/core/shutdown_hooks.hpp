#pragma once
// Shutdown hooks (spec 014 T3.3): an optional module that keeps engine memory between calls (the data index)
// registers a function that frees it; nnh::shutdown() runs them before the leak report. So the core never
// names optional modules, and a game that doesn't use them links none of their code. Internal; not
// thread-safe (the engine is single-threaded). Tests: tests/app/test_shutdown_hooks.cpp.

namespace nnh::detail {

inline constexpr int kMaxShutdownHooks = 8;

using ShutdownHook = void (*)();

/// Runs `hook` at the next nnh::shutdown(). Adding a hook already waiting does nothing (true). False when
/// kMaxShutdownHooks hooks are waiting.
bool add_shutdown_hook(ShutdownHook hook);

/// Runs the waiting hooks, newest first, and forgets them (nnh::shutdown()).
void run_shutdown_hooks();

} // namespace nnh::detail
