#pragma once
// After-input hooks (decision D32, spec 017 FR-15): an optional module that must run right after the frame's
// input sample (input actions) registers a node; nnh::frame() runs the list after the frame's input events are
// queued and before audio and dispatch. The head lives in app/app.cpp, so a game without hooks pays one null
// check and links none of add/remove. Nodes live inside their owner's storage; no allocation. Internal; the
// engine is single-threaded. Tests: tests/app/test_input_hooks.cpp.

namespace nnh::detail {

struct InputHook {
    InputHook* next = nullptr;
    void (*run)(InputHook& self) = nullptr; ///< set by the owner
};

/// The first hook (defined in app/app.cpp).
extern InputHook* g_input_hooks;

/// Runs every hook in registration order (nnh::frame()). A hook only reads input, updates its owner and
/// queues events: it never runs game code, so the list can't change during the pass.
inline void run_input_hooks() {
    for (InputHook* h = g_input_hooks; h != nullptr; h = h->next)
        h->run(*h);
}

/// Appends `hook` (asserts if it is already in the list).
void add_input_hook(InputHook& hook);
/// Removes `hook`; no-op if it is not in the list.
void remove_input_hook(InputHook& hook);

} // namespace nnh::detail
