// Action events (spec 017 FR-14): after an evaluation, every ActionPressed, then every ActionReleased, each in
// declaration order. Queued on the bus, so they are delivered in this frame's dispatch, after the input events.

#include "actions/actions_internal.hpp"
#include "nnh/events.hpp"

namespace nnh::detail {

void publish_action_events(const ActionBlock& b) {
    for (int i = 0; i < b.declared; ++i) {
        if ((b.actions[b.order[i]].flags & action_flag::kPressed) != 0) {
            events::publish(ActionPressed{b.serial, b.order[i]});
        }
    }
    for (int i = 0; i < b.declared; ++i) {
        if ((b.actions[b.order[i]].flags & action_flag::kReleased) != 0) {
            events::publish(ActionReleased{b.serial, b.order[i]});
        }
    }
}

} // namespace nnh::detail
