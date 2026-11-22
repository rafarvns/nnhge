#pragma once
// One slot's reading and its fold into the action's (spec 017 FR-6, FR-7; plan 017 "Semantics" and "Console bench
// refinement"), inline so that the per-frame loop (read_slots.cpp) has no call, no divide and no struct in memory
// per slot. eval_slot(), slot_engaged() and combine() are these functions out of line. Internal.

#include "actions/actions_internal.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>

namespace nnh::detail {

/// `p / 100.0f` for every byte, computed at compile time: the same bits as the division, without a VFP divide.
struct PercentTable {
    float v[256];
};
constexpr PercentTable make_percent_table() {
    PercentTable t{};
    for (int i = 0; i < 256; ++i) t.v[i] = static_cast<float>(i) / 100.0f;
    return t;
}
inline constexpr PercentTable kPercent = make_percent_table();

/// A threshold or dead zone in 1 % steps as a fraction.
inline float percent(std::uint8_t p) { return kPercent.v[p]; }

inline constexpr float kDiagonal = 0.70710678f;

inline bool down(const InputView& v, std::uint8_t index) { return index < 14 && (v.held >> index & 1u) != 0; }
inline float part(const InputView& v, std::uint8_t index) { return down(v, index) ? 1.0f : 0.0f; }
/// |v| as a value (keeps -0 and a NaN's sign, like the code it replaced).
inline float magnitude(float v) { return v < 0.0f ? -v : v; }
/// |v| for comparisons only: one `vabs`, no compare (differs from magnitude() only in the sign of -0 and NaN,
/// which no comparison sees).
inline float compare_magnitude(float v) { return std::fabs(v); }

/// `a >= b` for two floats known to be >= 0 and not NaN (+0 included, never -0), compared as integers: their bits
/// order like their values, and an ARM11 integer compare doesn't wait for the VFP pipeline (`vmrs`).
inline bool at_least(float a, float b) {
    std::int32_t x, y;
    std::memcpy(&x, &a, sizeof x);
    std::memcpy(&y, &b, sizeof y);
    return x >= y;
}

/// `value` past the extra dead zone `dz`, re-scaled to start at 0 and reach 1 (sign kept).
inline float rescale(float value, float dz) {
    if (dz <= 0.0f) return value;
    const float m = magnitude(value);
    if (m <= dz) return 0.0f;
    const float r = (m - dz) / (1.0f - dz);
    return value < 0.0f ? -r : r;
}

inline bool touched_inside(const SlotRec& s, const InputView& v) {
    return v.touching && v.touch_x >= s.rect[0] && v.touch_x < s.rect[0] + s.rect[2] && v.touch_y >= s.rect[1] &&
           v.touch_y < s.rect[1] + s.rect[3];
}

/// The push of a stick direction source (>= 0, never NaN or -0: see make_input_view()).
inline float direction_push(const PackedSource& s, const InputView& v) {
    return v.push[(s.d[0] & 1) * 4 + (s.d[1] & 3)];
}

/// A stick direction in a button action: held with the slot's hysteresis (updates its kHeld bit).
inline bool direction_held(SlotRec& slot, float push) {
    const bool was = (slot.state & slot_state::kHeld) != 0;
    const bool held = at_least(push, percent(was ? slot.release : slot.press));
    slot.state = static_cast<std::uint8_t>(held ? slot.state | slot_state::kHeld : slot.state & ~slot_state::kHeld);
    return held;
}

inline void read_whole_stick(const SlotRec& slot, const InputView& v, SlotOut& out) {
    const int stick = slot.cur.d[0] & 1;
    const Stick s = v.stick[stick];
    const float len = v.length[stick];
    const float scaled = rescale(len, percent(slot.dead_zone));
    const float k = len > 0.0f && scaled != len ? scaled / len : 1.0f;
    out.vec = {s.x * k, s.y * k};
    out.value = scaled;
}

inline void read_buttons4(const std::uint8_t* d, const InputView& v, SlotOut& out) {
    float x = part(v, d[3]) - part(v, d[2]);
    float y = part(v, d[0]) - part(v, d[1]);
    if (x != 0.0f && y != 0.0f) {
        x *= kDiagonal;
        y *= kDiagonal;
        out.value = 1.0f;
    } else {
        out.value = magnitude(x) + magnitude(y);
    }
    out.vec = {x, y};
}

/// The slot's reading for an action of `kind` (eval_slot() without the (value, 0) vector of button/axis actions,
/// which read_slots() sets once per action).
inline SlotOut read_slot(ActionKind kind, SlotRec& slot, const InputView& v) {
    SlotOut out;
    const std::uint8_t* d = slot.cur.d;
    switch (slot.cur.kind) {
    case Source::Kind::None: return out;
    case Source::Kind::Button:
        out.held = down(v, d[0]);
        out.value = out.held ? 1.0f : 0.0f;
        break;
    case Source::Kind::StickDirection:
        out.value = direction_push(slot.cur, v);
        if (kind == ActionKind::Button) out.held = direction_held(slot, out.value);
        break;
    case Source::Kind::Stick: read_whole_stick(slot, v, out); break;
    case Source::Kind::StickAxis: {
        const Stick s = v.stick[d[0] & 1];
        out.value = rescale(d[1] == 0 ? s.x : s.y, percent(slot.dead_zone));
        break;
    }
    case Source::Kind::Buttons2: out.value = part(v, d[1]) - part(v, d[0]); break;
    case Source::Kind::Buttons4: read_buttons4(d, v, out); break;
    case Source::Kind::Touch:
        out.held = touched_inside(slot, v);
        out.value = out.held ? 1.0f : 0.0f;
        break;
    }
    if (kind == ActionKind::Vector) {
        if ((slot.flags & slot_flag::kInvertX) != 0) out.vec.x = -out.vec.x;
        if ((slot.flags & slot_flag::kInvertY) != 0) out.vec.y = -out.vec.y;
    } else if (kind == ActionKind::Axis && (slot.flags & slot_flag::kInvertX) != 0) {
        out.value = -out.value;
    }
    return out;
}

/// Whether the slot's control is engaged at `threshold` (see slot_engaged()).
inline bool engaged(const SlotRec& slot, const InputView& v, float threshold) {
    const std::uint8_t* d = slot.cur.d;
    switch (slot.cur.kind) {
    case Source::Kind::None: return false;
    case Source::Kind::Button: return down(v, d[0]);
    case Source::Kind::StickDirection: return direction_push(slot.cur, v) >= threshold;
    case Source::Kind::Stick: return v.length[d[0] & 1] >= threshold;
    case Source::Kind::StickAxis: {
        const Stick s = v.stick[d[0] & 1];
        return compare_magnitude(d[1] == 0 ? s.x : s.y) >= threshold;
    }
    case Source::Kind::Buttons2: return down(v, d[0]) || down(v, d[1]);
    case Source::Kind::Buttons4: return down(v, d[0]) || down(v, d[1]) || down(v, d[2]) || down(v, d[3]);
    case Source::Kind::Touch: return touched_inside(slot, v);
    }
    return false;
}

inline float length_squared(Vec2 v) { return v.x * v.x + v.y * v.y; }

/// FR-7 for button actions: any-held, the largest value (into.value only grows from 0: never negative or NaN).
inline void fold_button(SlotOut& into, bool held, float value) {
    into.held = into.held || held;
    if (value > into.value) into.value = value;
}
/// FR-7 for axis actions: the largest |value|, ties keep the earlier slot.
inline void fold_axis(SlotOut& into, float value) {
    if (compare_magnitude(value) > compare_magnitude(into.value)) into.value = value;
}
/// FR-7 for vector actions: the longest vector; `best` is length_squared(into.vec), kept by the caller.
inline void fold_vector(SlotOut& into, float& best, const SlotOut& slot) {
    const float l = length_squared(slot.vec);
    if (l > best) {
        best = l;
        into.vec = slot.vec;
        into.value = slot.value;
    }
}

} // namespace nnh::detail
