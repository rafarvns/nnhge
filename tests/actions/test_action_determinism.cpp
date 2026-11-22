// Determinism (spec 017 T3.6, FR-22, AC-2): the same 120-frame input sequence, with a listen in the middle, run
// twice gives the same per-frame log of queries, events and listen results.

#include "actions_fixture.hpp"
#include "nnh/events.hpp"

#include <cstring>

using namespace actions_test;
using nnh::ActionKind;
using nnh::ActionSet;
using nnh::Source;

namespace {
enum class Act { Jump, Aim, Move };
constexpr int kFrames = 120;

struct Log {
    std::uint32_t frame_hash[kFrames + 1] = {};
    int events = 0;
    int binds = 0;
};

std::uint32_t mix(std::uint32_t h, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) {
        h ^= (v >> (i * 8)) & 0xFFu;
        h *= 0x01000193u;
    }
    return h;
}
std::uint32_t bits(float f) {
    std::uint32_t u;
    std::memcpy(&u, &f, sizeof u);
    return u;
}

Pad input_for(std::uint32_t& rng) {
    rng = rng * 1664525u + 1013904223u;
    Pad p;
    p.buttons = (rng >> 8) & (bit(Button::A) | bit(Button::B) | bit(Button::L) | bit(Button::R) | bit(Button::Up));
    p.cx = static_cast<float>(static_cast<int>((rng >> 16) % 21) - 10) / 10.0f;
    p.sy = static_cast<float>(static_cast<int>((rng >> 24) % 21) - 10) / 10.0f;
    return p;
}

void run(Log& log) {
    restart();
    ActionSet set{{.actions = 3}};
    CHECK(set.add(Act::Jump, {.name = "jump", .bindings = {btn(Button::A)}, .repeat = {.delay = 3, .interval = 2}}));
    CHECK(set.add(Act::Aim, {.name = "aim",
                             .kind = ActionKind::Axis,
                             .bindings = {{Source::stick_axis(nnh::AnalogStick::CStick, nnh::StickAxis::Y)},
                                          {Source::buttons(Button::L, Button::R)}}}));
    CHECK(set.add(Act::Move, {.name = "move",
                              .kind = ActionKind::Vector,
                              .bindings = {{Source::stick(nnh::AnalogStick::CirclePad)}, {Source::dpad()}}}));
    std::uint32_t events = 0x811C9DC5u;
    nnh::Subscription p = nnh::events::subscribe<nnh::ActionPressed>([&](const nnh::ActionPressed& e) {
        events = mix(events, e.action);
        ++log.events;
    });
    nnh::Subscription r = nnh::events::subscribe<nnh::ActionReleased>([&](const nnh::ActionReleased& e) {
        events = mix(events, 100u + e.action);
        ++log.events;
    });
    std::uint32_t rng = 7;
    for (int f = 1; f <= kFrames; ++f) {
        if (f == 60) set.listen(Act::Jump, 1, {.cancel = 0});
        frame(input_for(rng));
        std::uint32_t h = events;
        for (int a = 0; a < 3; ++a) {
            const nnh::ActionId id(a);
            h = mix(h, (set.pressed(id) ? 1u : 0u) | (set.held(id) ? 2u : 0u) | (set.released(id) ? 4u : 0u) |
                           (set.repeated(id) ? 8u : 0u));
            h = mix(h, bits(set.value(id)));
            h = mix(h, bits(set.vector(id).x));
            h = mix(h, bits(set.vector(id).y));
            h = mix(h, set.held_frames(id));
            h = mix(h, set.frames_since_pressed(id));
        }
        const nnh::RemapResult& res = set.listen_result();
        h = mix(h, static_cast<std::uint32_t>(res.state) | static_cast<std::uint32_t>(res.source.kind) << 8 |
                       static_cast<std::uint32_t>(res.source.a) << 16);
        if (res.state == nnh::ListenState::Bound) ++log.binds;
        log.frame_hash[f] = h;
    }
    nnh::shutdown();
}
} // namespace

NNH_TEST(the_same_input_sequence_gives_the_same_actions_events_and_listen_results) {
    static Log first, second;
    first = Log{};
    second = Log{};
    run(first);
    run(second);
    CHECK(std::memcmp(first.frame_hash, second.frame_hash, sizeof first.frame_hash) == 0);
    CHECK(first.events == second.events && first.events > 10); // the sequence really moved things
    CHECK(first.binds > 0);                                     // the listen captured something
}
