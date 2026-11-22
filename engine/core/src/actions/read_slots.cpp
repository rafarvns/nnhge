// An action's slots read and combined for one evaluation (spec 017 FR-6, FR-7, FR-13; plan 017 "Console bench
// refinement"): one loop per action kind, every slot inline. Controls at rest are skipped with integer tests where
// their reading could not change the result, so an idle frame costs a few loads per slot. Gives exactly what
// eval_slot() + combine() give, slot by slot (test_action_eval_golden.cpp).

#include "actions/slot_read.hpp"

namespace nnh::detail {
namespace {

using Kind = Source::Kind;

/// FR-13: a blocked slot reads nothing until its control is released below its release threshold.
bool readable(SlotRec& s, const InputView& v) {
    if ((s.flags & slot_flag::kActive) == 0) return false;
    if ((s.state & slot_state::kBlocked) == 0) return true;
    if (engaged(s, v, percent(s.release))) return false;
    s.state = static_cast<std::uint8_t>(s.state & ~slot_state::kBlocked);
    return true;
}

bool is_zero(float f) {
    std::uint32_t u;
    std::memcpy(&u, &f, sizeof u);
    return u == 0;
}

SlotOut read_button_action(ActionRec& r, const InputView& v) {
    SlotOut acc; // acc.value only grows from +0: never negative, -0 or NaN, so at_least() may compare it
    for (SlotRec& s : r.slots) {
        if (!readable(s, v)) continue;
        switch (s.cur.kind) {
        case Kind::Button:
            if (down(v, s.cur.d[0])) fold_button(acc, true, 1.0f); // up: (false, 0) changes nothing
            break;
        case Kind::Touch:
            if (touched_inside(s, v)) fold_button(acc, true, 1.0f);
            break;
        case Kind::StickDirection: {
            const float push = direction_push(s.cur, v);
            acc.held = direction_held(s, push) || acc.held;
            if (!at_least(acc.value, push)) acc.value = push;
            break;
        }
        default: {
            const SlotOut o = read_slot(ActionKind::Button, s, v);
            fold_button(acc, o.held, o.value);
        }
        }
    }
    acc.vec = {acc.value, 0.0f};
    return acc;
}

SlotOut read_axis_action(ActionRec& r, const InputView& v) {
    SlotOut acc;
    for (SlotRec& s : r.slots) {
        if (!readable(s, v)) continue;
        const std::uint8_t* d = s.cur.d;
        float value;
        switch (s.cur.kind) {
        case Kind::StickAxis: {
            const Stick st = v.stick[d[0] & 1];
            const float c = d[1] == 0 ? st.x : st.y;
            if (is_zero(compare_magnitude(c))) continue; // reads +-0, and |0| never beats the larger |value| kept
            value = rescale(c, percent(s.dead_zone));
            break;
        }
        case Kind::StickDirection:
            value = direction_push(s.cur, v);
            if (is_zero(value)) continue;
            break;
        case Kind::Buttons2: {
            const int n = (down(v, d[1]) ? 1 : 0) - (down(v, d[0]) ? 1 : 0);
            if (n == 0) continue;
            value = static_cast<float>(n);
            break;
        }
        default: fold_axis(acc, read_slot(ActionKind::Axis, s, v).value); continue;
        }
        fold_axis(acc, (s.flags & slot_flag::kInvertX) != 0 ? -value : value);
    }
    acc.vec = {acc.value, 0.0f};
    return acc;
}

/// Invert after everything else (FR-8).
void invert(const SlotRec& s, SlotOut& out) {
    if ((s.flags & slot_flag::kInvertX) != 0) out.vec.x = -out.vec.x;
    if ((s.flags & slot_flag::kInvertY) != 0) out.vec.y = -out.vec.y;
}

SlotOut read_vector_action(ActionRec& r, const InputView& v) {
    SlotOut acc;
    float best = 0.0f; // length_squared(acc.vec)
    for (SlotRec& s : r.slots) {
        if (!readable(s, v)) continue;
        const std::uint8_t* d = s.cur.d;
        SlotOut o;
        // At rest the reading is a zero vector (length_squared exactly 0), which never wins.
        switch (s.cur.kind) {
        case Kind::Stick:
            if (is_zero(v.length[d[0] & 1])) continue;
            read_whole_stick(s, v, o);
            break;
        case Kind::Buttons4:
            if (down(v, d[0]) == down(v, d[1]) && down(v, d[2]) == down(v, d[3])) continue;
            read_buttons4(d, v, o);
            break;
        default: fold_vector(acc, best, read_slot(ActionKind::Vector, s, v)); continue;
        }
        invert(s, o);
        fold_vector(acc, best, o);
    }
    return acc;
}

} // namespace

SlotOut read_slots(ActionRec& r, const InputView& v) {
    switch (r.kind) {
    case ActionKind::Button: return read_button_action(r, v);
    case ActionKind::Axis: return read_axis_action(r, v);
    case ActionKind::Vector: return read_vector_action(r, v);
    }
    return {};
}

} // namespace nnh::detail
