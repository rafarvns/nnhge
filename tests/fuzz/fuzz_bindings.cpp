// Fuzz target body for saved input bindings `NNHB` (spec 017 T5.3, FR-21; see fuzz_targets.hpp): validated as
// given, then with the header fixed so mutations reach the entries; every accepted blob is applied to a fixed set
// with every action kind, saved again, and the saved blob must load back whole and save to the same bytes.

#include "fuzz_targets.hpp"

#include <cstdlib>
#include <cstring>

#include "actions/bindings_format.hpp"
#include "nnh/actions.hpp"

namespace nnh::fuzz {

namespace {

enum class Act { Jump, Attack, Pause, Tap, Aim, Steer, Move, Look };
enum class Ctx { Play, Menu };

ActionSet make_set() {
    ActionSet set{{.actions = 8, .contexts = 2}};
    set.add_context(Ctx::Menu, "menu");
    set.add(Act::Jump, {.name = "jump", .bindings = {{Source::button(Button::A)}, {Source::button(Button::ZL)}}});
    set.add(Act::Attack, {.name = "attack",
                          .context = Ctx::Menu,
                          .bindings = {{Source::stick_direction(AnalogStick::CStick, Direction::Up)}}});
    set.add(Act::Pause, {.name = "pause", .bindings = {{.source = Source::button(Button::Start), .locked = true}}});
    set.add(Act::Tap, {.name = "tap", .bindings = {{Source::touch(0, 0, 32, 32)}}});
    set.add(Act::Aim, {.name = "aim",
                       .kind = ActionKind::Axis,
                       .bindings = {{Source::stick_axis(AnalogStick::CStick, StickAxis::X)},
                                    {.source = Source::buttons(Button::L, Button::R), .fallback = true}}});
    set.add(Act::Steer, {.name = "steer",
                         .kind = ActionKind::Axis,
                         .bindings = {{Source::stick_direction(AnalogStick::CirclePad, Direction::Right)}}});
    set.add(Act::Move, {.name = "move",
                        .kind = ActionKind::Vector,
                        .bindings = {{Source::stick(AnalogStick::CirclePad)}, {Source::dpad()}}});
    set.add(Act::Look, {.name = "look", .kind = ActionKind::Vector, .bindings = {{Source::stick(AnalogStick::CStick)}}});
    return set;
}

std::uint8_t g_fixed[kMaxBindingsSize];
std::uint8_t g_saved[kMaxBindingsSize];
std::uint8_t g_again[kMaxBindingsSize];

/// Applies an accepted blob, then checks that what the set saves loads back whole and saves the same bytes.
void apply_and_round_trip(ActionSet& set, const std::uint8_t* blob, std::size_t size) {
    const Result<LoadReport> r = set.load_from(blob, size);
    if (!r.ok()) std::abort(); // validated just before
    const Result<std::size_t> saved = set.save_to(g_saved, sizeof g_saved);
    if (!saved.ok()) std::abort();
    const Result<LoadReport> back = set.load_from(g_saved, saved.value());
    if (!back.ok() || back.value().skipped != 0) std::abort();
    const Result<std::size_t> again = set.save_to(g_again, sizeof g_again);
    if (!again.ok() || again.value() != saved.value() || std::memcmp(g_saved, g_again, saved.value()) != 0) {
        std::abort();
    }
    char text[64];
    for (int a = 0; a < 8; ++a) {
        (void)set.describe(ActionId(a), text, sizeof text);
        Conflict out[8];
        (void)set.conflicts(ActionId(a), out, 8);
    }
}

} // namespace

std::size_t bindings_seed(std::uint8_t* out, std::size_t capacity) {
    ActionSet set = make_set();
    set.set_source(Act::Jump, 0, Source::button(Button::X));
    set.set_part(Act::Move, 1, Part::Left, Button::Y);
    set.set_source(Act::Aim, 0, Source::stick_axis(AnalogStick::CirclePad, StickAxis::Y), ConflictPolicy::Allow);
    set.set_invert(Act::Look, 0, true, true);
    set.clear(Act::Steer, 0);
    const Result<std::size_t> size = set.save_to(out, capacity, 0);
    return size.ok() ? size.value() : 0;
}

bool bindings_fixed_accepted(const std::uint8_t* data, std::size_t size) {
    if (size < detail::bindings::kHeaderSize || size > kMaxBindingsSize) return false;
    const int count = static_cast<int>((size - detail::bindings::kHeaderSize) / detail::bindings::kEntrySize);
    const std::size_t fixed = detail::bindings::kHeaderSize + detail::bindings::kEntrySize * static_cast<std::size_t>(count);
    std::memcpy(g_fixed, data, fixed);
    detail::bindings::finish_blob(g_fixed, count, 0);
    int n = 0;
    std::uint32_t version = 0;
    return detail::bindings::validate(g_fixed, fixed, n, version) == detail::bindings::Check::Ok;
}

int bindings_target(const std::uint8_t* data, std::size_t size) {
    ActionSet set = make_set(); // per input: a set registers an input hook, which must not outlive the call
    int count = 0;
    std::uint32_t version = 0;
    if (detail::bindings::validate(data, size, count, version) == detail::bindings::Check::Ok) {
        (void)set.load_from(data, size, version);
        apply_and_round_trip(set, data, size);
    } else {
        (void)set.load_from(data, size, 0); // refused: must not change anything or read out of bounds
    }
    if (bindings_fixed_accepted(data, size)) {
        apply_and_round_trip(set, g_fixed, detail::bindings::kHeaderSize +
                                          detail::bindings::kEntrySize *
                                              ((size - detail::bindings::kHeaderSize) / detail::bindings::kEntrySize));
    }
    return 0;
}

} // namespace nnh::fuzz
