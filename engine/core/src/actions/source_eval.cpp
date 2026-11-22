// One slot's reading per source kind (spec 017 FR-6; plan 017 "Semantics"): pure functions of the input view.

#include "actions/actions_internal.hpp"

namespace nnh::detail {
namespace {

constexpr float kDiagonal = 0.70710678f;

bool down(const InputView& v, std::uint8_t index) {
    return index < 14 && (v.held & (1u << index)) != 0;
}
float part(const InputView& v, std::uint8_t index) { return down(v, index) ? 1.0f : 0.0f; }
float magnitude(float v) { return v < 0.0f ? -v : v; }

/// `value` past the extra dead zone `dz`, re-scaled to start at 0 and reach 1 (sign kept).
float rescale(float value, float dz) {
    if (dz <= 0.0f) return value;
    const float m = magnitude(value);
    if (m <= dz) return 0.0f;
    const float r = (m - dz) / (1.0f - dz);
    return value < 0.0f ? -r : r;
}

bool touched_inside(const SlotRec& s, const InputView& v) {
    return v.touching && v.touch_x >= s.rect[0] && v.touch_x < s.rect[0] + s.rect[2] && v.touch_y >= s.rect[1] &&
           v.touch_y < s.rect[1] + s.rect[3];
}

void read_stick(SlotRec& slot, const InputView& v, SlotOut& out) {
    const int stick = slot.cur.d[0] & 1;
    const Stick s = v.stick[stick];
    const float len = v.length[stick];
    const float scaled = rescale(len, slot.dead_zone / 100.0f);
    const float k = len > 0.0f && scaled != len ? scaled / len : 1.0f;
    out.vec = {s.x * k, s.y * k};
    out.value = scaled;
}

void read_buttons4(const std::uint8_t* d, const InputView& v, SlotOut& out) {
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

void read_direction(ActionKind kind, SlotRec& slot, const InputView& v, SlotOut& out) {
    const float push = v.push[(slot.cur.d[0] & 1) * 4 + (slot.cur.d[1] & 3)];
    out.value = push;
    if (kind != ActionKind::Button) return;
    const bool was = (slot.state & slot_state::kHeld) != 0;
    out.held = push >= (was ? slot.release : slot.press) / 100.0f;
    slot.state = static_cast<std::uint8_t>(out.held ? slot.state | slot_state::kHeld
                                                    : slot.state & ~slot_state::kHeld);
}

} // namespace

SlotOut eval_slot(ActionKind kind, SlotRec& slot, const InputView& v) {
    SlotOut out;
    const std::uint8_t* d = slot.cur.d;
    switch (slot.cur.kind) {
    case Source::Kind::None: return out;
    case Source::Kind::Button:
        out.held = down(v, d[0]);
        out.value = out.held ? 1.0f : 0.0f;
        break;
    case Source::Kind::StickDirection: read_direction(kind, slot, v, out); break;
    case Source::Kind::Stick: read_stick(slot, v, out); break;
    case Source::Kind::StickAxis: {
        const Stick s = v.stick[d[0] & 1];
        out.value = rescale(d[1] == 0 ? s.x : s.y, slot.dead_zone / 100.0f);
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
    } else {
        if (kind == ActionKind::Axis && (slot.flags & slot_flag::kInvertX) != 0) out.value = -out.value;
        out.vec = {out.value, 0.0f};
    }
    return out;
}

bool slot_engaged(const SlotRec& slot, const InputView& v, float threshold) {
    const std::uint8_t* d = slot.cur.d;
    switch (slot.cur.kind) {
    case Source::Kind::None: return false;
    case Source::Kind::Button: return down(v, d[0]);
    case Source::Kind::StickDirection: return v.push[(d[0] & 1) * 4 + (d[1] & 3)] >= threshold;
    case Source::Kind::Stick: return v.length[d[0] & 1] >= threshold;
    case Source::Kind::StickAxis: {
        const Stick s = v.stick[d[0] & 1];
        return magnitude(d[1] == 0 ? s.x : s.y) >= threshold;
    }
    case Source::Kind::Buttons2: return down(v, d[0]) || down(v, d[1]);
    case Source::Kind::Buttons4: return down(v, d[0]) || down(v, d[1]) || down(v, d[2]) || down(v, d[3]);
    case Source::Kind::Touch: return touched_inside(slot, v);
    }
    return false;
}

} // namespace nnh::detail
