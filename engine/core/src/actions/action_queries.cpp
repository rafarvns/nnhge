// Reading actions (spec 017 FR-9, FR-11, FR-12, FR-14): the values of the last evaluation. An empty set reads
// idle; an unknown id asserts in Debug/Development and reads idle (with a log) in Release.

#include "actions/actions_internal.hpp"
#include "nnh/assert.hpp"

namespace nnh {
namespace detail {

const ActionRec* find_action(const ActionBlock* b, ActionId id) {
    if (b == nullptr) return nullptr;
    if (id.index < b->capacity && (b->actions[id.index].flags & action_flag::kDeclared) != 0) {
        return &b->actions[id.index];
    }
    NNH_LOG_ERROR(actions, "ActionSet: action id %u is not declared (ignored)", static_cast<unsigned>(id.index));
    NNH_ASSERT(false, "nnh::ActionSet: action id %u is not declared", static_cast<unsigned>(id.index));
    return nullptr;
}

ActionRec* find_action(ActionBlock* b, ActionId id) {
    return const_cast<ActionRec*>(find_action(static_cast<const ActionBlock*>(b), id));
}

SlotRec* find_slot(ActionBlock* b, ActionId id, int slot) {
    ActionRec* r = find_action(b, id);
    if (r == nullptr) return nullptr;
    if (slot >= 0 && slot < kMaxSlots) return &r->slots[slot];
    NNH_LOG_ERROR(actions, "ActionSet: slot %d of \"%s\" outside 0..%d (ignored)", slot, r->name, kMaxSlots - 1);
    NNH_ASSERT(false, "nnh::ActionSet: slot %d outside 0..%d", slot, kMaxSlots - 1);
    return nullptr;
}

namespace {
bool flag(const ActionBlock* b, ActionId a, std::uint8_t f) {
    const ActionRec* r = find_action(b, a);
    return r != nullptr && (r->flags & f) != 0;
}
} // namespace
} // namespace detail

bool ActionSet::pressed(ActionId a) const { return detail::flag(block_, a, detail::action_flag::kPressed); }
bool ActionSet::held(ActionId a) const { return detail::flag(block_, a, detail::action_flag::kHeld); }
bool ActionSet::released(ActionId a) const { return detail::flag(block_, a, detail::action_flag::kReleased); }
bool ActionSet::repeated(ActionId a) const { return detail::flag(block_, a, detail::action_flag::kRepeated); }

float ActionSet::value(ActionId a) const {
    const detail::ActionRec* r = detail::find_action(block_, a);
    return r != nullptr ? r->value : 0.0f;
}

Vec2 ActionSet::vector(ActionId a) const {
    const detail::ActionRec* r = detail::find_action(block_, a);
    return r != nullptr ? r->out : Vec2{};
}

std::uint32_t ActionSet::held_frames(ActionId a) const {
    const detail::ActionRec* r = detail::find_action(block_, a);
    return r != nullptr ? r->held_frames : 0;
}

std::uint32_t ActionSet::frames_since_pressed(ActionId a) const {
    const detail::ActionRec* r = detail::find_action(block_, a);
    return r != nullptr ? r->since_pressed : 0xFFFFFFFFu;
}

bool ActionSet::usable(ActionId a) const {
    const detail::ActionRec* r = detail::find_action(block_, a);
    if (r == nullptr) return false;
    for (const detail::SlotRec& s : r->slots) {
        if ((s.flags & detail::slot_flag::kActive) != 0) return true;
    }
    return false;
}

void ActionSet::set_events_enabled(bool on) {
    if (block_ != nullptr) block_->events = on;
}

Source ActionSet::source(ActionId a, int slot) const {
    if (block_ == nullptr) return {};
    const detail::SlotRec* s = detail::find_slot(block_, a, slot);
    return s != nullptr ? detail::unpack(*s) : Source{};
}

const char* ActionSet::name(ActionId a) const {
    if (block_ == nullptr || a.index >= block_->capacity) return "";
    const detail::ActionRec& r = block_->actions[a.index];
    return (r.flags & detail::action_flag::kDeclared) != 0 ? r.name : "";
}

} // namespace nnh
