// The set's after-input hook (decision D32, spec 017 FR-15): registered at creation, removed at destruction. Each
// nnh::frame() evaluates the set from this frame's public input queries and queues its events.

#include "actions/actions_internal.hpp"

#include <cstddef>
#include <type_traits>

namespace nnh::detail {
namespace {

static_assert(std::is_standard_layout_v<ActionBlock>, "the hook's block is found with offsetof");

void on_input(InputHook& hook) {
    auto& b = *reinterpret_cast<ActionBlock*>(reinterpret_cast<unsigned char*>(&hook) - offsetof(ActionBlock, hook));
    evaluate(b, current_input_view());
    if (b.events) publish_action_events(b);
}

} // namespace

void actions_attach(ActionBlock& b) {
    b.hook.run = &on_input;
    add_input_hook(b.hook);
}

void actions_detach(ActionBlock& b) { remove_input_hook(b.hook); }

} // namespace nnh::detail
