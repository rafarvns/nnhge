#pragma once
// After-input hooks (decision D32, spec 017 FR-15): an optional module that must run right after the frame's
// input sample (input actions) registers a node; nnh::frame() runs the list after the frame's input events are
// queued and before audio and dispatch. app/app.cpp holds one function pointer, set while the list is not empty,
// so a game without hooks pays one load and a null check and links none of this file's code (plan 017 stage 3
// refinements: the inline loop cost 40 bytes in hello). Nodes live inside their owner's storage; no allocation.
// Internal; the engine is single-threaded. Tests: tests/app/test_input_hooks.cpp.

namespace nnh::detail {

struct InputHook {
    InputHook* next = nullptr;
    void (*run)(InputHook& self) = nullptr; ///< set by the owner
};

/// Runs the hooks; null while there are none (defined in app/app.cpp, called by nnh::frame()).
extern void (*g_run_input_hooks)();

/// The first hook (input_hooks.cpp).
extern InputHook* g_input_hooks;

/// Runs every hook in registration order. A hook only reads input, updates its owner and queues events: it
/// never runs game code, so the list can't change during the pass.
void run_input_hooks();

/// Appends `hook` (asserts if it is already in the list).
void add_input_hook(InputHook& hook);
/// Removes `hook`; no-op if it is not in the list.
void remove_input_hook(InputHook& hook);

} // namespace nnh::detail
