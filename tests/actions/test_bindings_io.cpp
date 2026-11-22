// Saving and loading bindings (spec 017 T5.2, FR-20, FR-21, AC-5): round trips through a buffer and nnh::save,
// reordered enums and new actions, game versions, every skip reason counted, damaged, too-new and missing files,
// ZL kept on an Old 3DS.

#include "actions/bindings_format.hpp"
#include "actions_fixture.hpp"
#include "nnh/detail/name_hash.hpp"

#include <cstring>

using namespace actions_test;
using nnh::ActionKind;
using nnh::ActionSet;
using nnh::AnalogStick;
using nnh::Part;
using nnh::Source;
using nnh::StickAxis;
namespace fmt = nnh::detail::bindings;

namespace {
enum class Act { Jump, Attack, Move, Pause };
enum class Act2 { Extra, Attack, Jump, Move, Pause }; // a later release: reordered, one action added

constexpr const char* kPath = "controls.bin";
std::uint8_t g_blob[nnh::kMaxBindingsSize];

ActionSet make() {
    ActionSet set{{.actions = 4}};
    CHECK(set.add(Act::Jump, {.name = "jump", .bindings = {btn(Button::A), btn(Button::Up)}}));
    CHECK(set.add(Act::Attack, {.name = "attack", .bindings = {btn(Button::B)}}));
    CHECK(set.add(Act::Move, {.name = "move",
                              .kind = ActionKind::Vector,
                              .bindings = {{Source::stick(AnalogStick::CirclePad)},
                                           {Source::buttons(Button::X, Button::Down, Button::Left, Button::Right)}}}));
    CHECK(set.add(Act::Pause, {.name = "pause", .bindings = {{.source = Source::button(Button::Start), .locked = true}}}));
    return set;
}

void change(ActionSet& set) {
    set.set_source(Act::Jump, 0, Source::button(Button::Y));
    set.set_part(Act::Move, 1, Part::Up, Button::L);
    set.set_invert(Act::Move, 0, false, true);
}

bool changed(const ActionSet& set) {
    return set.source(Act::Jump, 0) == Source::button(Button::Y) &&
           set.source(Act::Move, 1) == Source::buttons(Button::L, Button::Down, Button::Left, Button::Right);
}

bool defaults(const ActionSet& set) {
    return set.source(Act::Jump, 0) == Source::button(Button::A) &&
           set.source(Act::Move, 1) == Source::buttons(Button::X, Button::Down, Button::Left, Button::Right);
}

std::size_t blob_of(const ActionSet& set, std::uint32_t version = 0) {
    const nnh::Result<std::size_t> r = set.save_to(g_blob, sizeof g_blob, version);
    CHECK(r.ok());
    return r.ok() ? r.value() : 0;
}

/// A blob of the given entries (header fixed).
std::size_t blob_with(const fmt::Entry* entries, int count, std::uint32_t version = 0) {
    for (int i = 0; i < count; ++i) fmt::encode_entry(entries[i], g_blob + fmt::kHeaderSize + i * fmt::kEntrySize);
    fmt::finish_blob(g_blob, count, version);
    return fmt::kHeaderSize + fmt::kEntrySize * static_cast<std::size_t>(count);
}
} // namespace

NNH_TEST(bindings_round_trip_through_a_buffer) {
    restart();
    ActionSet set = make();
    change(set);
    const std::size_t size = blob_of(set);
    set.reset_all();
    CHECK(defaults(set));
    const nnh::Result<nnh::LoadReport> r = set.load_from(g_blob, size);
    CHECK(r.ok() && r.value().found && r.value().applied == 3 && r.value().skipped == 0);
    CHECK(changed(set));
    frame();
    frame({.cy = 1.0f});
    CHECK(set.vector(Act::Move).y < 0.0f); // the invert came back too
    nnh::shutdown();
}

NNH_TEST(bindings_round_trip_through_a_save_file) {
    restart();
    {
        ActionSet set = make();
        change(set);
        CHECK(set.save(kPath, 3).ok());
    }
    ActionSet fresh = make();
    const nnh::Result<nnh::LoadReport> r = fresh.load(kPath, 3);
    CHECK(r.ok() && r.value().found && r.value().applied == 3);
    CHECK(changed(fresh));
    nnh::shutdown();
}

NNH_TEST(loading_resets_what_the_file_does_not_change) {
    restart();
    ActionSet set = make();
    const std::size_t empty = blob_of(set);
    change(set);
    CHECK(set.load_from(g_blob, empty).value().applied == 0);
    CHECK(defaults(set));
    nnh::shutdown();
}

NNH_TEST(a_missing_file_is_no_remaps) {
    restart();
    ActionSet set = make();
    change(set);
    const nnh::Result<nnh::LoadReport> r = set.load("nothing.bin");
    CHECK(r.ok() && !r.value().found && r.value().applied == 0);
    CHECK(changed(set)); // nothing changes
    nnh::shutdown();
}

NNH_TEST(a_damaged_file_changes_nothing) {
    restart();
    ActionSet set = make();
    change(set);
    CHECK(set.save(kPath).ok()); // first write: no backup
    nnh::platform_fake::corrupt_file(kPath, 20);
    set.reset_all();
    const nnh::Result<nnh::LoadReport> r = set.load(kPath);
    CHECK(!r.ok() && r.error().code == nnh::ErrorCode::Corrupted);
    CHECK(defaults(set));
    nnh::shutdown();
}

NNH_TEST(a_truncated_or_too_new_blob_changes_nothing) {
    restart();
    ActionSet set = make();
    change(set);
    std::size_t size = blob_of(set);
    set.reset_all();
    nnh::Result<nnh::LoadReport> r = set.load_from(g_blob, size - 1);
    CHECK(!r.ok() && r.error().code == nnh::ErrorCode::Corrupted && defaults(set));
    g_blob[4] = fmt::kFormat + 1;
    r = set.load_from(g_blob, size);
    CHECK(!r.ok() && r.error().code == nnh::ErrorCode::Unsupported && defaults(set));
    CHECK(!set.load_from(nullptr, 0).ok());
    nnh::shutdown();
}

NNH_TEST(another_game_bindings_version_is_ignored) {
    restart();
    ActionSet set = make();
    change(set);
    const std::size_t size = blob_of(set, 1);
    set.reset_all();
    const nnh::Result<nnh::LoadReport> r = set.load_from(g_blob, size, 2);
    CHECK(r.ok() && r.value().version_ignored && r.value().applied == 0);
    CHECK(defaults(set));
    nnh::shutdown();
}

NNH_TEST(a_reordered_enum_and_new_actions_keep_the_players_choices) {
    restart();
    std::size_t size = 0;
    {
        ActionSet old = make();
        change(old);
        size = blob_of(old);
    }
    ActionSet later{{.actions = 5}};
    CHECK(later.add(Act2::Extra, {.name = "extra", .bindings = {btn(Button::R)}}));
    CHECK(later.add(Act2::Attack, {.name = "attack", .bindings = {btn(Button::X)}})); // new default
    CHECK(later.add(Act2::Jump, {.name = "jump", .bindings = {btn(Button::A), btn(Button::Up)}}));
    CHECK(later.add(Act2::Move, {.name = "move",
                                 .kind = ActionKind::Vector,
                                 .bindings = {{Source::stick(AnalogStick::CirclePad)},
                                              {Source::buttons(Button::X, Button::Down, Button::Left, Button::Right)}}}));
    const nnh::Result<nnh::LoadReport> r = later.load_from(g_blob, size);
    CHECK(r.ok() && r.value().applied == 3);
    CHECK(later.source(Act2::Jump, 0) == Source::button(Button::Y));
    CHECK(later.source(Act2::Move, 1) == Source::buttons(Button::L, Button::Down, Button::Left, Button::Right));
    CHECK(later.source(Act2::Attack, 0) == Source::button(Button::X)); // the new default
    CHECK(later.source(Act2::Extra, 0) == Source::button(Button::R));
    nnh::shutdown();
}

NNH_TEST(every_unfitting_entry_is_skipped_counted_and_warned_about) {
    restart();
    ActionSet set = make();
    const std::uint32_t jump = nnh::detail::fnv1a32("jump");
    const std::uint32_t move = nnh::detail::fnv1a32("move");
    const std::uint8_t button = static_cast<std::uint8_t>(Source::Kind::Button);
    const fmt::Entry entries[] = {
        {.hash = jump, .slot = 1, .kind = button, .data = {3}},                         // applied: jump slot 1 = Y
        {.hash = 0x12345678u, .kind = button},                                          // unknown action
        {.hash = jump, .slot = 4, .kind = button},                                      // slot out of range
        {.hash = jump, .slot = 2, .part = 3, .kind = button},                           // part on a button action
        {.hash = move, .slot = 1, .part = 7, .kind = button},                           // no such part
        {.hash = jump, .slot = 2, .kind = 9},                                           // unknown kind
        {.hash = jump, .slot = 2, .kind = static_cast<std::uint8_t>(Source::Kind::Stick)}, // doesn't fit
        {.hash = jump, .slot = 2, .kind = button, .data = {14}},                        // button out of range
        {.hash = jump, .slot = 2, .kind = button, .data = {1, 1}},                      // unused byte set
        {.hash = jump, .slot = 2, .kind = button, .flags = 4},                          // unknown flag
        {.hash = jump, .slot = 2, .kind = static_cast<std::uint8_t>(Source::Kind::Touch)}, // touch
        {.hash = nnh::detail::fnv1a32("pause"), .kind = button, .data = {2}},          // locked
        {.hash = jump, .slot = 1, .kind = button, .data = {2}},                         // repeated key
        {.hash = move, .slot = 1, .part = 3, .kind = button, .flags = 1},               // flags on a part
    };
    const std::size_t size = blob_with(entries, 14);
    const int lines = nnh::platform_fake::state().console_lines;
    const nnh::Result<nnh::LoadReport> r = set.load_from(g_blob, size);
    CHECK(r.ok() && r.value().applied == 1 && r.value().skipped == 13);
    CHECK(set.source(Act::Jump, 1) == Source::button(Button::Y));
    CHECK(set.source(Act::Pause, 0) == Source::button(Button::Start));
    CHECK(nnh::platform_fake::state().console_lines == lines + 1 && last_log_mentions("13"));
    nnh::shutdown();
}

NNH_TEST(a_part_entry_turns_a_simple_slot_into_a_composite) {
    restart();
    ActionSet set = make();
    const fmt::Entry e{.hash = nnh::detail::fnv1a32("move"), .slot = 0, .part = 5, .kind = 1, .data = {4}};
    CHECK(set.load_from(g_blob, blob_with(&e, 1)).value().applied == 1);
    Source expected = Source::buttons(Button::A, Button::A, Button::L, Button::A);
    expected.a = expected.b = expected.d = Source::kNoButton;
    CHECK(set.source(Act::Move, 0) == expected);
    nnh::shutdown();
}

NNH_TEST(a_zl_binding_loaded_on_an_old_3ds_is_kept_inactive_and_saved_again) {
    restart();
    std::size_t size = 0;
    {
        ActionSet set = make();
        set.set_source(Act::Attack, 0, Source::button(Button::ZL));
        size = blob_of(set);
    }
    static std::uint8_t first[nnh::kMaxBindingsSize];
    std::memcpy(first, g_blob, size);
    restart(/*old_3ds=*/true);
    ActionSet set = make();
    CHECK(set.load_from(first, size).value().applied == 1);
    CHECK(set.source(Act::Attack, 0) == Source::button(Button::ZL) && !set.usable(Act::Attack));
    CHECK(blob_of(set) == size && std::memcmp(first, g_blob, size) == 0);
    nnh::shutdown();
}
