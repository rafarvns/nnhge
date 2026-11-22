#pragma once
// A mixed evaluation workload (spec 017, console bench refinement): the actions example's bench set (64 actions x 4
// slots of every source kind) driven by a deterministic input sequence, with a hash of every public reading. Shared
// by the golden test (test_action_eval_golden.cpp) and the PC microbenchmark (bench_actions.cpp).

#include "actions/actions_internal.hpp"
#include "nnh/actions.hpp"

#include <cstdint>
#include <cstring>

namespace actions_workload {

using nnh::AnalogStick;
using nnh::Button;
using nnh::Direction;
using nnh::Source;

inline constexpr int kActions = 64;

inline Button button(int i) { return static_cast<Button>(1u << (i % 14)); }

/// The bench scene's declaration of action `i` (examples/actions/bench_scene.cpp). `varied` adds what the bench
/// leaves out: a second context, menu repeat and other thresholds.
inline nnh::ActionDesc desc(int i, const char* name, bool varied) {
    const auto stick = static_cast<AnalogStick>(i & 1);
    const auto dir = static_cast<Direction>(i % 4);
    nnh::ActionDesc d{.name = name};
    switch (i % 3) {
    case 0:
        d.kind = nnh::ActionKind::Button;
        d.bindings[0] = {Source::button(button(i))};
        d.bindings[1] = {Source::stick_direction(stick, dir)};
        d.bindings[2] = {Source::button(button(i + 5))};
        d.bindings[3] = {Source::touch(i * 4, 0, 16, 16)};
        break;
    case 1:
        d.kind = nnh::ActionKind::Axis;
        d.bindings[0] = {Source::stick_axis(stick, nnh::StickAxis::X)};
        d.bindings[1] = {Source::buttons(button(i), button(i + 1))};
        d.bindings[2] = {Source::stick_direction(stick, dir)};
        d.bindings[3] = {.source = Source::stick_axis(stick, nnh::StickAxis::Y), .dead_zone = 0.2f};
        break;
    default:
        d.kind = nnh::ActionKind::Vector;
        d.bindings[0] = {.source = Source::stick(stick), .dead_zone = 0.1f};
        d.bindings[1] = {Source::dpad()};
        d.bindings[2] = {Source::buttons(Button::X, Button::B, Button::Y, Button::A)};
        d.bindings[3] = {.source = Source::stick(stick), .invert_y = true};
        break;
    }
    if (varied) {
        if (i % 5 == 0) d.context = nnh::ContextId(1);
        if (i % 3 == 0) {
            d.repeat = {.delay = static_cast<std::uint8_t>(i % 7), .interval = static_cast<std::uint8_t>(1 + i % 4)};
        }
        if (i % 4 == 1) {
            d.press = 0.3f;
            d.release = 0.25f;
            d.bindings[2].press = 0.7f;
            d.bindings[2].release = 0.1f;
            d.bindings[2].invert_x = true;
        }
    }
    return d;
}

/// Declares the workload's actions into `set` (made with {.actions = 64, .contexts = 2} when `varied`).
inline void declare(nnh::ActionSet& set, bool varied) {
    if (varied) set.add_context(nnh::ContextId(1), "second");
    for (int i = 0; i < kActions; ++i) {
        char name[4] = {'b', static_cast<char>('0' + i / 10), static_cast<char>('0' + i % 10), '\0'};
        set.add(nnh::ActionId(i), desc(i, name, varied));
    }
    set.set_events_enabled(false);
}

/// Stick components that hit the thresholds and dead zones exactly, and some in between.
inline constexpr float kComponents[] = {0.0f, 0.0f, 0.0f,  0.1f,  -0.1f, 0.2f,  -0.2f, 0.25f, 0.3f,  -0.3f, 0.4f,
                                        -0.4f, 0.41f, 0.5f, -0.5f, 0.6f,  0.7f,  -0.7f, 0.75f, 1.0f,  -1.0f, 0.39f,
                                        0.49f, -0.6f, 0.21f, 0.11f, -0.8f, 0.9f,  0.33f, -0.95f, 0.66f, 0.05f};

struct Lcg {
    std::uint32_t s = 12345;
    std::uint32_t next() {
        s = s * 1664525u + 1013904223u;
        return s >> 8;
    }
};

/// The view of frame `f`: inputs change every few frames so that holds, repeats and hysteresis happen.
inline nnh::detail::InputView view_for(Lcg& rng) {
    const std::uint32_t r0 = rng.next(), r1 = rng.next(), r2 = rng.next();
    const std::uint32_t held = (r0 & r1 & 0x3FFFu) | (r2 & 0x2000u ? 0x10u : 0u);
    const nnh::Stick circle{kComponents[r1 & 31], kComponents[(r1 >> 5) & 31]};
    const nnh::Stick c{kComponents[(r2 >> 3) & 31], kComponents[(r2 >> 8) & 31]};
    const bool touching = (r0 >> 14 & 3) == 0;
    const nnh::TouchPoint t{static_cast<int>(r2 % 320), static_cast<int>(r1 % 24)};
    return nnh::detail::make_input_view(held, circle, c, touching, t);
}

inline std::uint32_t mix(std::uint32_t h, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) {
        h ^= (v >> (i * 8)) & 0xFFu;
        h *= 0x01000193u;
    }
    return h;
}

inline std::uint32_t bits(float f) {
    std::uint32_t u;
    std::memcpy(&u, &f, sizeof u);
    return u;
}

/// Every public reading of every action.
inline std::uint32_t hash_readings(const nnh::ActionSet& set, std::uint32_t h) {
    for (int a = 0; a < kActions; ++a) {
        const nnh::ActionId id(a);
        h = mix(h, (set.pressed(id) ? 1u : 0u) | (set.held(id) ? 2u : 0u) | (set.released(id) ? 4u : 0u) |
                       (set.repeated(id) ? 8u : 0u));
        h = mix(h, bits(set.value(id)));
        h = mix(h, bits(set.vector(id).x));
        h = mix(h, bits(set.vector(id).y));
        h = mix(h, set.held_frames(id));
        h = mix(h, set.frames_since_pressed(id));
    }
    return h;
}

} // namespace actions_workload
