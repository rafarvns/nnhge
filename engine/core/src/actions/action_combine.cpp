// Combining an action's slots (spec 017 FR-7) and the axis/vector held hysteresis (FR-3). Pure.

#include "actions/actions_internal.hpp"

namespace nnh::detail {
namespace {

float magnitude(float v) { return v < 0.0f ? -v : v; }
float length_squared(Vec2 v) { return v.x * v.x + v.y * v.y; }

} // namespace

void combine(ActionKind kind, SlotOut& into, const SlotOut& slot) {
    switch (kind) {
    case ActionKind::Button:
        into.held = into.held || slot.held;
        if (slot.value > into.value) into.value = slot.value;
        into.vec = {into.value, 0.0f};
        break;
    case ActionKind::Axis:
        if (magnitude(slot.value) > magnitude(into.value)) {
            into.value = slot.value;
            into.vec = {slot.value, 0.0f};
        }
        break;
    case ActionKind::Vector:
        if (length_squared(slot.vec) > length_squared(into.vec)) {
            into.vec = slot.vec;
            into.value = slot.value;
        }
        break;
    }
}

bool held_with_hysteresis(float magnitude_now, bool was_held, float press, float release) {
    return magnitude_now >= (was_held ? release : press);
}

} // namespace nnh::detail
