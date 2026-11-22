// After-input hooks (input_hooks.hpp, D32): the list, its pass, adding and removing nodes. Linked only by the
// modules that register hooks; app/app.cpp sees only g_run_input_hooks.

#include "core/input_hooks.hpp"

#include "nnh/assert.hpp"

namespace nnh::detail {

InputHook* g_input_hooks = nullptr;

void run_input_hooks() {
    for (InputHook* h = g_input_hooks; h != nullptr; h = h->next)
        h->run(*h);
}

void add_input_hook(InputHook& hook) {
    InputHook** link = &g_input_hooks;
    for (; *link != nullptr; link = &(*link)->next) {
        if (*link == &hook) {
            NNH_ASSERT(false, "input hook added twice");
            return; // Release: already there (appending again would make a loop)
        }
    }
    hook.next = nullptr;
    *link = &hook;
    g_run_input_hooks = &run_input_hooks;
}

void remove_input_hook(InputHook& hook) {
    for (InputHook** link = &g_input_hooks; *link != nullptr; link = &(*link)->next) {
        if (*link == &hook) {
            *link = hook.next;
            hook.next = nullptr;
            break;
        }
    }
    if (g_input_hooks == nullptr) g_run_input_hooks = nullptr;
}

} // namespace nnh::detail
