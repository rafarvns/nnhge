// Same sample, every input path (spec 017 T3.6, FR-10, AC-2): the fake platform's controls and spec 013's remote
// input give the same action results frame by frame; the D-Pad mirror drives D-Pad bindings when it is on.
// Debug/Development only (the remote session is compiled out of Release).

#include "../remote/remote_fixture.hpp"
#include "actions_fixture.hpp"
#include "nnh/input.hpp"

using namespace actions_test;
using nnh::ActionKind;
using nnh::ActionSet;
using nnh::Source;

namespace {
enum class Act { Jump, Move, Tap };

ActionSet make() {
    ActionSet set{{.actions = 3}};
    CHECK(set.add(Act::Jump, {.name = "jump", .bindings = {btn(Button::A)}}));
    CHECK(set.add(Act::Move, {.name = "move",
                              .kind = ActionKind::Vector,
                              .bindings = {{Source::stick(nnh::AnalogStick::CirclePad)}, {Source::dpad()}}}));
    CHECK(set.add(Act::Tap, {.name = "tap", .bindings = {{Source::touch(0, 0, 40, 40)}}}));
    return set;
}

constexpr int kFrames = 10;

/// One frame's results, packed: edges and held of every action, and the move vector in thousandths.
struct Row {
    std::uint32_t bits = 0;
    int mx = 0, my = 0;
    friend bool operator==(const Row&, const Row&) = default;
};

Row row(const ActionSet& set) {
    Row r;
    for (int a = 0; a < 3; ++a) {
        const nnh::ActionId id(a);
        r.bits |= (set.pressed(id) ? 1u : 0u) << (a * 3);
        r.bits |= (set.held(id) ? 2u : 0u) << (a * 3);
        r.bits |= (set.released(id) ? 4u : 0u) << (a * 3);
    }
    r.mx = static_cast<int>(set.vector(Act::Move).x * 1000.0f + 0.5f);
    r.my = static_cast<int>(set.vector(Act::Move).y * 1000.0f + 0.5f);
    return r;
}

/// The script: A held on frames 3-4, Circle Pad right on 4-5, touch (10, 10) on 7.
Pad pad_for(int f) {
    Pad p;
    if (f == 3 || f == 4) p.buttons = bit(Button::A);
    if (f == 4 || f == 5) p.cx = 1.0f;
    if (f == 7) {
        p.touch = true;
        p.tx = 10;
        p.ty = 10;
    }
    return p;
}

void start(bool remote) {
    nnh::shutdown();
    nnh::platform_fake::reset();
    nnh::platform_fake::set_capabilities({Capability::SecondScreen, Capability::TouchScreen, Capability::CirclePad});
    if (remote) nnh::platform_fake::remote_connect();
    CHECK(nnh::init({}));
}

void send_script() {
    using namespace remote_test;
    send(hello());
    send(buttons(2, 3, bit(Button::A)));
    send(stick(3, 4, 0, 1000, 0));
    send(buttons(4, 5, 0, bit(Button::A)));
    send(stick(5, 6, 0, 0, 0));
    send(touch(6, 7, true, 10, 10));
    send(touch(7, 8, false, 10, 10));
}
} // namespace

NNH_TEST(remote_input_drives_actions_exactly_like_local_input) {
    Row local[kFrames + 1], remote[kFrames + 1];
    start(false);
    {
        ActionSet set = make();
        for (int f = 1; f <= kFrames; ++f) {
            frame(pad_for(f));
            local[f] = row(set);
        }
    }
    start(true);
    {
        ActionSet set = make();
        send_script();
        for (int f = 1; f <= kFrames; ++f) {
            frame();
            remote[f] = row(set);
        }
    }
    nnh::shutdown();
    for (int f = 1; f <= kFrames; ++f) CHECK(local[f] == remote[f]);
    CHECK(local[3].bits == 3u && local[4].mx == 1000 && local[7].bits == (1u << 6 | 2u << 6)); // the script ran
}

NNH_TEST(the_d_pad_mirror_drives_d_pad_bindings_only_when_on) {
    restart();
    ActionSet set{{.actions = 1}};
    CHECK(set.add(Act::Jump, {.name = "right", .bindings = {btn(Button::Right)}}));
    frame();
    frame({.cx = 1.0f});
    CHECK(!set.pressed(Act::Jump));
    frame();
    nnh::input::set_circle_pad_mirrors_dpad(true);
    frame({.cx = 1.0f});
    CHECK(set.pressed(Act::Jump));
    nnh::shutdown();
}
