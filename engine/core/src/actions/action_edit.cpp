// Editing bindings (spec 017 FR-8, FR-18, FR-19): set a source or a part with a conflict policy, clear, invert,
// reset a slot, an action, a context or the set to the declared defaults; locked slots never change. describe()
// of an action. Changes apply from the next evaluation.

#include "actions/actions_internal.hpp"
#include "nnh/assert.hpp"

namespace nnh {
namespace detail {
namespace {

constexpr std::uint8_t kInvert = slot_flag::kInvertX | slot_flag::kInvertY;

RemapResult refused(const char* why, const ActionRec* r, int slot) {
    NNH_LOG_ERROR(actions, "ActionSet: can't remap \"%s\" slot %d: %s (ignored)", r != nullptr ? r->name : "", slot,
                  why);
    NNH_ASSERT(false, "nnh::ActionSet: can't remap: %s", why);
    RemapResult result;
    result.state = ListenState::Rejected;
    return result;
}

/// The slot if it may be changed by the player's settings (asserts on a locked one).
SlotRec* remappable(ActionBlock* b, ActionId a, int slot) {
    SlotRec* s = find_slot(b, a, slot);
    if (s == nullptr) return nullptr;
    if ((s->flags & slot_flag::kLocked) != 0) {
        refused("the slot is locked", &b->actions[a.index], slot);
        return nullptr;
    }
    return s;
}

void reset_slot(SlotRec& s, std::uint8_t caps) {
    if ((s.flags & slot_flag::kLocked) != 0) return;
    s.cur = s.def;
    const auto inverts = static_cast<std::uint8_t>(((s.flags & slot_flag::kDefInvertX) != 0 ? slot_flag::kInvertX : 0) |
                                                   ((s.flags & slot_flag::kDefInvertY) != 0 ? slot_flag::kInvertY : 0));
    s.flags = static_cast<std::uint8_t>((s.flags & ~kInvert) | inverts);
    slot_changed(s, caps);
}

void reset_action(ActionRec& r, std::uint8_t caps) {
    for (SlotRec& s : r.slots) reset_slot(s, caps);
}

} // namespace
} // namespace detail

RemapResult ActionSet::set_source(ActionId a, int slot, Source source, ConflictPolicy policy) {
    if (detail::find_slot(block_, a, slot) == nullptr) return {};
    const detail::ActionRec& r = block_->actions[a.index];
    if (const char* why = detail::remap_problem(r, slot, Part::Whole, source)) return detail::refused(why, &r, slot);
    return detail::assign(*block_, a.index, slot, Part::Whole, source, policy);
}

RemapResult ActionSet::set_part(ActionId a, int slot, Part part, Button button, ConflictPolicy policy) {
    if (detail::find_slot(block_, a, slot) == nullptr) return {};
    const detail::ActionRec& r = block_->actions[a.index];
    const Source source = Source::button(button);
    if (const char* why = detail::remap_problem(r, slot, part, source)) return detail::refused(why, &r, slot);
    return detail::assign(*block_, a.index, slot, part, source, policy);
}

void ActionSet::clear(ActionId a, int slot) {
    if (detail::SlotRec* s = detail::remappable(block_, a, slot)) {
        s->cur = {};
        detail::slot_changed(*s, block_->caps);
    }
}

void ActionSet::set_invert(ActionId a, int slot, bool invert_x, bool invert_y) {
    if (detail::SlotRec* s = detail::remappable(block_, a, slot)) {
        const auto flags = static_cast<std::uint8_t>((invert_x ? detail::slot_flag::kInvertX : 0) |
                                                     (invert_y ? detail::slot_flag::kInvertY : 0));
        s->flags = static_cast<std::uint8_t>((s->flags & ~detail::kInvert) | flags);
    }
}

void ActionSet::reset(ActionId a, int slot) {
    if (detail::SlotRec* s = detail::find_slot(block_, a, slot)) detail::reset_slot(*s, block_->caps);
}

void ActionSet::reset(ActionId a) {
    if (detail::ActionRec* r = detail::find_action(block_, a)) detail::reset_action(*r, block_->caps);
}

void ActionSet::reset_context(ContextId id) {
    if (block_ == nullptr) return;
    for (int i = 0; i < block_->declared; ++i) {
        detail::ActionRec& r = block_->actions[block_->order[i]];
        if (r.context == id.index) detail::reset_action(r, block_->caps);
    }
}

void ActionSet::reset_all() {
    if (block_ == nullptr) return;
    for (int i = 0; i < block_->declared; ++i) detail::reset_action(block_->actions[block_->order[i]], block_->caps);
}

std::size_t ActionSet::describe(ActionId a, char* out, std::size_t capacity) const {
    if (out == nullptr || capacity == 0) return 0;
    out[0] = '\0';
    const detail::ActionRec* r = detail::find_action(block_, a);
    if (r == nullptr) return 0;
    std::size_t n = 0;
    for (const detail::SlotRec& s : r->slots) {
        if ((s.flags & detail::slot_flag::kActive) == 0) continue;
        if (n > 0 && n + 1 < capacity) {
            const char sep[] = " / ";
            for (int i = 0; sep[i] != '\0' && n + 1 < capacity; ++i) out[n++] = sep[i];
            out[n] = '\0';
        }
        n += actions::describe(detail::unpack(s), out + n, capacity - n);
    }
    return n;
}

} // namespace nnh
