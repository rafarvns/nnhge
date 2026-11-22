// Conflicts (spec 017 FR-17; plan 017 "Semantics"): a binding unit (action, slot, part) reads a footprint of
// controls; two units of one context conflict when their footprints meet. A new source is swapped, replaced,
// allowed or rejected; locked units win unless the policy is Allow.

#include "actions/actions_internal.hpp"

namespace nnh {
namespace detail {
namespace {

struct Unit {
    std::uint8_t action, slot;
    Part part;
};

/// Calls `fn(unit, locked)` for each unit of `self`'s context, other than `self` (and the rest of its slot when
/// `self` is Whole), whose footprint meets `mask`; in action, slot, part order.
template <typename Fn>
void for_each_hit(ActionBlock& b, Unit self, std::uint32_t mask, Fn&& fn) {
    if (mask == 0) return;
    const int context = b.actions[self.action].context;
    for (int a = 0; a < b.capacity; ++a) {
        ActionRec& r = b.actions[a];
        if ((r.flags & action_flag::kDeclared) == 0 || r.context != context) continue;
        for (int s = 0; s < kMaxSlots; ++s) {
            Part parts[4];
            const int n = units_of(r.slots[s].cur, parts);
            for (int u = 0; u < n; ++u) {
                const bool same_slot = a == self.action && s == self.slot;
                // The unit itself, and what it replaces: the whole slot, or a simple source turning composite.
                if (same_slot && (self.part == Part::Whole || parts[u] == self.part || parts[u] == Part::Whole)) {
                    continue;
                }
                if ((footprint(r.slots[s].cur, parts[u]) & mask) == 0) continue;
                fn(Unit{static_cast<std::uint8_t>(a), static_cast<std::uint8_t>(s), parts[u]},
                   (r.slots[s].flags & slot_flag::kLocked) != 0);
            }
        }
    }
}

Source::Kind composite_kind(ActionKind kind) {
    return kind == ActionKind::Axis ? Source::Kind::Buttons2 : Source::Kind::Buttons4;
}

/// The unit's source as a value to hand over: a whole source, or a part's button as a Button source.
PackedSource unit_source(const ActionRec& r, int slot, Part part) {
    const PackedSource& cur = r.slots[slot].cur;
    if (part == Part::Whole) return cur;
    PackedSource p;
    if (cur.kind == composite_kind(r.kind) && cur.d[part_index(part)] != Source::kNoButton) {
        p.kind = Source::Kind::Button;
        p.d[0] = cur.d[part_index(part)];
    }
    return p;
}

/// Writes `value` (whole, or a Button for a part; None clears) into a unit. False if it doesn't fit.
bool put(ActionBlock& b, Unit u, const PackedSource& value) {
    ActionRec& r = b.actions[u.action];
    SlotRec& s = r.slots[u.slot];
    if (u.part == Part::Whole) {
        if (!source_fits(r.kind, value.kind) || value.kind == Source::Kind::Touch) return false;
        s.cur = value;
    } else {
        if (value.kind != Source::Kind::None && value.kind != Source::Kind::Button) return false;
        if (s.cur.kind != composite_kind(r.kind)) {
            s.cur = PackedSource{composite_kind(r.kind), {Source::kNoButton, Source::kNoButton, 0, 0}};
            if (r.kind == ActionKind::Vector) s.cur.d[2] = s.cur.d[3] = Source::kNoButton;
        }
        s.cur.d[part_index(u.part)] = value.kind == Source::Kind::Button ? value.d[0] : Source::kNoButton;
    }
    slot_changed(s, b.caps);
    return true;
}

void clear_unit(ActionBlock& b, Unit u) { put(b, u, PackedSource{}); }

} // namespace

int part_index(Part part) {
    switch (part) {
    case Part::Negative:
    case Part::Up: return 0;
    case Part::Positive:
    case Part::Down: return 1;
    case Part::Left: return 2;
    case Part::Right: return 3;
    case Part::Whole: break;
    }
    return 0;
}

int units_of(const PackedSource& source, Part out[4]) {
    switch (source.kind) {
    case Source::Kind::None: return 0;
    case Source::Kind::Buttons2:
        out[0] = Part::Negative;
        out[1] = Part::Positive;
        return 2;
    case Source::Kind::Buttons4:
        out[0] = Part::Up;
        out[1] = Part::Down;
        out[2] = Part::Left;
        out[3] = Part::Right;
        return 4;
    default: out[0] = Part::Whole; return 1;
    }
}

const char* remap_problem(const ActionRec& r, int slot, Part part, const Source& source) {
    if ((r.slots[slot].flags & slot_flag::kLocked) != 0) return "the slot is locked";
    if (source.kind == Source::Kind::Touch) return "touch areas can't be remapped";
    if (part == Part::Whole) {
        if (!source_fits(r.kind, source.kind)) return "the source doesn't fit the action kind";
    } else {
        if (!part_fits(r.kind, part)) return "the part doesn't fit the action kind";
        if (source.kind != Source::Kind::Button) return "a composite part takes a button";
    }
    const PackedSource p = pack(source);
    if (!source_data_valid(p) || (p.kind == Source::Kind::Button && p.d[0] == Source::kNoButton)) {
        return "the source is invalid";
    }
    return nullptr;
}

RemapResult assign(ActionBlock& b, int action, int slot, Part part, const Source& source, ConflictPolicy policy) {
    RemapResult r;
    r.state = ListenState::Bound;
    r.action = static_cast<std::uint16_t>(action);
    r.slot = static_cast<std::uint8_t>(slot);
    r.part = part;
    r.source = source;
    const Unit self{static_cast<std::uint8_t>(action), static_cast<std::uint8_t>(slot), part};
    const std::uint32_t mask = footprint(pack(source), Part::Whole); // a part's source is its button
    int count = 0;
    bool locked = false;
    for_each_hit(b, self, mask, [&](Unit u, bool is_locked) {
        if (count++ == 0) {
            r.other_action = u.action;
            r.other_slot = u.slot;
            r.other_part = u.part;
        }
        locked = locked || is_locked;
    });
    r.conflicts = static_cast<std::uint8_t>(count > 255 ? 255 : count);
    if (count > 0 && policy != ConflictPolicy::Allow && (locked || policy == ConflictPolicy::Reject)) {
        r.state = ListenState::Rejected;
        r.resolution = Resolution::Rejected;
        return r;
    }
    const PackedSource previous = unit_source(b.actions[action], slot, part);
    put(b, self, pack(source));
    if (count == 0) return r;
    if (policy == ConflictPolicy::Allow) {
        r.resolution = Resolution::Allowed;
        return r;
    }
    r.resolution = Resolution::Replaced;
    bool first = true;
    for_each_hit(b, self, mask, [&](Unit u, bool) {
        if (first && policy == ConflictPolicy::Swap && previous.kind != Source::Kind::None && put(b, u, previous)) {
            r.resolution = Resolution::Swapped;
        } else {
            clear_unit(b, u);
        }
        first = false;
    });
    return r;
}

} // namespace detail

int ActionSet::conflicts(ActionId a, Conflict* out, int capacity) const {
    detail::ActionRec* r = detail::find_action(block_, a);
    if (r == nullptr) return 0;
    int count = 0;
    for (int s = 0; s < kMaxSlots; ++s) {
        Part parts[4];
        const int n = detail::units_of(r->slots[s].cur, parts);
        for (int u = 0; u < n; ++u) {
            const detail::Unit self{static_cast<std::uint8_t>(a.index), static_cast<std::uint8_t>(s), parts[u]};
            detail::for_each_hit(*block_, self, detail::footprint(r->slots[s].cur, parts[u]),
                                 [&](detail::Unit hit, bool) {
                                     if (out != nullptr && count < capacity) {
                                         out[count] = {static_cast<std::uint8_t>(s), parts[u], hit.action, hit.slot,
                                                       hit.part};
                                     }
                                     ++count;
                                 });
        }
    }
    return count;
}

} // namespace nnh
