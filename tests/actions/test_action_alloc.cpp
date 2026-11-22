// No allocation after creation (spec 017 T5.4, FR-1, FR-23, AC-6): evaluating, events, listening, remapping,
// resetting, saving and loading make no operator new call and no nnh::mem allocation; the leak report is clean once
// the set is gone. (save(path)/load(path) are checked on engine memory only: the fake platform's files live in
// std containers.)

#include "actions_fixture.hpp"
#include "alloc_counter.hpp"
#include "nnh/events.hpp"
#include "nnh/memory.hpp"

using namespace actions_test;
using nnh::ActionKind;
using nnh::ActionSet;
using nnh::Source;
namespace counter = nnh::test::alloc_counter;

namespace {
enum class Act { Jump, Aim, Move, Tap };

ActionSet make() {
    ActionSet set{{.actions = 4, .contexts = 2}};
    CHECK(set.add_context(nnh::ContextId(1), "menu"));
    CHECK(set.add(Act::Jump, {.name = "jump", .bindings = {btn(Button::A), btn(Button::B)}, .repeat = {4, 2}}));
    CHECK(set.add(Act::Aim, {.name = "aim",
                             .kind = ActionKind::Axis,
                             .bindings = {{Source::stick_axis(nnh::AnalogStick::CStick, nnh::StickAxis::X)},
                                          {Source::buttons(Button::L, Button::R)}}}));
    CHECK(set.add(Act::Move, {.name = "move",
                              .kind = ActionKind::Vector,
                              .context = nnh::ContextId(1),
                              .bindings = {{Source::stick(nnh::AnalogStick::CirclePad)}, {Source::dpad()}}}));
    CHECK(set.add(Act::Tap, {.name = "tap", .bindings = {{Source::touch(0, 0, 20, 20)}}}));
    return set;
}

std::uint8_t g_blob[nnh::kMaxBindingsSize];

void exercise(ActionSet& set) {
    int seen = 0;
    nnh::Subscription sub = nnh::events::subscribe<nnh::ActionPressed>([&](const nnh::ActionPressed&) { ++seen; });
    for (int round = 0; round < 10; ++round) {
        frame({.buttons = bit(Button::A) | bit(Button::L), .cx = 1.0f, .touch = true, .tx = 5, .ty = 5});
        frame();
        set.enable(nnh::ContextId(1), round % 2 == 0);
        set.listen(Act::Jump, 2);
        frame();
        frame({.buttons = bit(Button::Y)});
        set.listen(Act::Move, 1, {.part = nnh::Part::Up});
        frame({.buttons = bit(Button::X)});
        set.set_source(Act::Jump, 0, Source::button(Button::X));
        set.set_part(Act::Aim, 1, nnh::Part::Negative, Button::ZL);
        set.set_invert(Act::Aim, 0, true, false);
        nnh::Conflict out[8];
        (void)set.conflicts(Act::Jump, out, 8);
        char text[32];
        (void)set.describe(Act::Jump, text, sizeof text);
        const nnh::Result<std::size_t> size = set.save_to(g_blob, sizeof g_blob);
        set.reset(Act::Jump);
        set.reset_context(nnh::ContextId(1));
        set.reset_all();
        if (size.ok()) (void)set.load_from(g_blob, size.value());
    }
    CHECK(seen > 0);
}
} // namespace

NNH_TEST(actions_allocate_nothing_after_creation) {
    restart();
    const int live_before = nnh::mem::live_allocations();
    const std::size_t tag_before = nnh::mem::tag_stats(nnh::detail::actions_memory_tag()).bytes;
    {
        ActionSet set = make();
        const int live = nnh::mem::live_allocations();
        CHECK(live == live_before + 1);
        frame();
        counter::reset();
        counter::enable(true);
        exercise(set);
        counter::enable(false);
        CHECK(counter::count() == 0);
        CHECK(nnh::mem::live_allocations() == live);
        CHECK(set.save("controls.bin").ok()); // engine memory only (see above)
        CHECK(set.load("controls.bin").ok());
        CHECK(nnh::mem::live_allocations() == live);
    }
    CHECK(nnh::mem::live_allocations() == live_before);
    CHECK(nnh::mem::tag_stats(nnh::detail::actions_memory_tag()).bytes == tag_before);
    nnh::shutdown();
}
