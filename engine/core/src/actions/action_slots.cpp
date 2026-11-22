// Slot records (plan 017 "Data and memory"): packing sources, which sources fit which actions, which slots are
// active on this system (FR-9), and the controls a binding reads (FR-17 footprints). Pure.

#include "actions/actions_internal.hpp"

namespace nnh::detail {
namespace {

bool has(std::uint8_t caps, Capability c) { return (caps & (1u << static_cast<unsigned>(c))) != 0; }

bool button_available(std::uint8_t index, std::uint8_t caps) {
    if (index >= 14) return false;
    return (index != 6 && index != 7) || has(caps, Capability::ZButtons); // ZL, ZR
}

bool stick_available(std::uint8_t stick, std::uint8_t caps) {
    return has(caps, stick == 0 ? Capability::CirclePad : Capability::CStick);
}

bool source_available(const PackedSource& s, std::uint8_t caps) {
    switch (s.kind) {
    case Source::Kind::None: return false;
    case Source::Kind::Button: return button_available(s.d[0], caps);
    case Source::Kind::StickDirection:
    case Source::Kind::Stick:
    case Source::Kind::StickAxis: return stick_available(s.d[0], caps);
    case Source::Kind::Buttons2: return button_available(s.d[0], caps) || button_available(s.d[1], caps);
    case Source::Kind::Buttons4:
        for (int i = 0; i < 4; ++i) {
            if (button_available(s.d[i], caps)) return true;
        }
        return false;
    case Source::Kind::Touch: return has(caps, Capability::TouchScreen);
    }
    return false;
}

std::uint32_t button_bit(std::uint8_t index) { return index < 14 ? 1u << index : 0u; }
std::uint32_t direction_bits(int stick, unsigned mask) { return static_cast<std::uint32_t>(mask) << (16 + stick * 4); }

} // namespace

std::uint8_t to_percent(float v) {
    if (!(v > 0.0f)) return 0;
    if (v >= 1.0f) return 100;
    return static_cast<std::uint8_t>(v * 100.0f + 0.5f);
}

bool source_fits(ActionKind kind, Source::Kind source) {
    using K = Source::Kind;
    if (source == K::None) return true;
    switch (kind) {
    case ActionKind::Button: return source == K::Button || source == K::StickDirection || source == K::Touch;
    case ActionKind::Axis: return source == K::StickAxis || source == K::StickDirection || source == K::Buttons2;
    case ActionKind::Vector: return source == K::Stick || source == K::Buttons4;
    }
    return false;
}

bool part_fits(ActionKind kind, Part part) {
    if (kind == ActionKind::Axis) return part == Part::Negative || part == Part::Positive;
    if (kind == ActionKind::Vector) return part >= Part::Up && part <= Part::Right;
    return false;
}

bool source_data_valid(const PackedSource& s) {
    const std::uint8_t* d = s.d;
    const auto part = [](std::uint8_t i) { return i < 14 || i == Source::kNoButton; };
    switch (s.kind) {
    case Source::Kind::None: return d[0] == 0 && d[1] == 0 && d[2] == 0 && d[3] == 0;
    case Source::Kind::Button: return d[0] < 14 && d[1] == 0 && d[2] == 0 && d[3] == 0;
    case Source::Kind::StickDirection: return d[0] < 2 && d[1] < 4 && d[2] == 0 && d[3] == 0;
    case Source::Kind::Stick: return d[0] < 2 && d[1] == 0 && d[2] == 0 && d[3] == 0;
    case Source::Kind::StickAxis: return d[0] < 2 && d[1] < 2 && d[2] == 0 && d[3] == 0;
    case Source::Kind::Buttons2: return part(d[0]) && part(d[1]) && d[2] == 0 && d[3] == 0;
    case Source::Kind::Buttons4: return part(d[0]) && part(d[1]) && part(d[2]) && part(d[3]);
    case Source::Kind::Touch: return true;
    }
    return false;
}

PackedSource pack(const Source& s) {
    PackedSource p;
    p.kind = s.kind;
    p.d[0] = s.a;
    p.d[1] = s.b;
    p.d[2] = s.c;
    p.d[3] = s.d;
    return p;
}

Source unpack(const SlotRec& slot) {
    Source s;
    s.kind = slot.cur.kind;
    s.a = slot.cur.d[0];
    s.b = slot.cur.d[1];
    s.c = slot.cur.d[2];
    s.d = slot.cur.d[3];
    if (s.kind == Source::Kind::Touch) {
        s.x = slot.rect[0];
        s.y = slot.rect[1];
        s.w = slot.rect[2];
        s.h = slot.rect[3];
    }
    return s;
}

SlotRec make_slot(const Binding& b, std::uint8_t caps) {
    SlotRec s;
    s.cur = s.def = pack(b.source);
    s.press = to_percent(b.press);
    s.release = to_percent(b.release);
    s.dead_zone = to_percent(b.dead_zone);
    const bool touch = b.source.kind == Source::Kind::Touch;
    s.flags = static_cast<std::uint8_t>((b.locked || touch ? slot_flag::kLocked : 0) |
                                        (b.fallback ? slot_flag::kFallback : 0) |
                                        (b.invert_x ? slot_flag::kInvertX | slot_flag::kDefInvertX : 0) |
                                        (b.invert_y ? slot_flag::kInvertY | slot_flag::kDefInvertY : 0));
    s.fallback_for = b.fallback_for;
    if (touch) {
        s.rect[0] = b.source.x;
        s.rect[1] = b.source.y;
        s.rect[2] = b.source.w;
        s.rect[3] = b.source.h;
    }
    refresh_slot(s, caps);
    return s;
}

void refresh_slot(SlotRec& slot, std::uint8_t caps) {
    bool active = source_available(slot.cur, caps);
    if ((slot.flags & slot_flag::kFallback) != 0 && has(caps, slot.fallback_for)) active = false;
    slot.flags = static_cast<std::uint8_t>(active ? slot.flags | slot_flag::kActive : slot.flags & ~slot_flag::kActive);
}

std::uint32_t footprint(const SlotRec& slot, Part part) {
    const std::uint8_t* d = slot.cur.d;
    switch (slot.cur.kind) {
    case Source::Kind::None:
    case Source::Kind::Touch: return 0;
    case Source::Kind::Button: return button_bit(d[0]);
    case Source::Kind::StickDirection: return direction_bits(d[0] & 1, 1u << (d[1] & 3));
    case Source::Kind::Stick: return direction_bits(d[0] & 1, 0xFu);
    case Source::Kind::StickAxis: return direction_bits(d[0] & 1, d[1] == 0 ? 0xCu : 0x3u);
    case Source::Kind::Buttons2:
        if (part == Part::Negative) return button_bit(d[0]);
        if (part == Part::Positive) return button_bit(d[1]);
        return button_bit(d[0]) | button_bit(d[1]);
    case Source::Kind::Buttons4:
        if (part >= Part::Up && part <= Part::Right) return button_bit(d[static_cast<int>(part) - 3]);
        return button_bit(d[0]) | button_bit(d[1]) | button_bit(d[2]) | button_bit(d[3]);
    }
    return 0;
}

} // namespace nnh::detail
