// The NNHB bindings blob (spec 017 T5.1, FR-20): byte-exact output for a known set of changes, only changed
// slots stored, part entries for one changed composite part, structural validation.

#include "actions/bindings_format.hpp"
#include "actions_fixture.hpp"

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
enum class Act { Jump, Aim, Move, Pause };

// Written by build-independent Python (zlib.crc32, FNV-1a 32): jump slot 0 = X; move slot 1 Up = Y (a part
// entry); aim slot 0 inverted. Game bindings version 7. Entries sorted by name hash.
constexpr std::uint8_t kGolden[] = {
    0x4E, 0x4E, 0x48, 0x42, 0x01, 0x00, 0x03, 0x00, 0x07, 0x00, 0x00, 0x00, 0xE3, 0xA0, 0x35, 0x5B, 0x14, 0x00,
    0x4B, 0x18, 0x01, 0x03, 0x01, 0x00, 0x03, 0x00, 0x00, 0x00, 0xF6, 0x51, 0x32, 0x38, 0x00, 0x00, 0x04, 0x01,
    0x01, 0x00, 0x00, 0x00, 0x0D, 0x5C, 0x3F, 0xA7, 0x00, 0x00, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00};

ActionSet make() {
    ActionSet set{{.actions = 4}};
    CHECK(set.add(Act::Jump, {.name = "jump", .bindings = {btn(Button::A), btn(Button::B)}}));
    CHECK(set.add(Act::Aim, {.name = "aim",
                             .kind = ActionKind::Axis,
                             .bindings = {{Source::stick_axis(AnalogStick::CStick, StickAxis::X)},
                                          {Source::buttons(Button::L, Button::R)}}}));
    CHECK(set.add(Act::Move, {.name = "move",
                              .kind = ActionKind::Vector,
                              .bindings = {{Source::stick(AnalogStick::CirclePad)}, {Source::dpad()}}}));
    CHECK(set.add(Act::Pause, {.name = "pause", .bindings = {{.source = Source::button(Button::Start), .locked = true}}}));
    return set;
}

std::uint8_t g_out[nnh::kMaxBindingsSize];
} // namespace

NNH_TEST(saving_writes_the_documented_bytes) {
    restart();
    ActionSet set = make();
    set.set_source(Act::Jump, 0, Source::button(Button::X));
    set.set_part(Act::Move, 1, Part::Up, Button::Y);
    set.set_invert(Act::Aim, 0, true, false);
    const nnh::Result<std::size_t> size = set.save_to(g_out, sizeof g_out, 7);
    CHECK(size.ok() && size.value() == sizeof kGolden);
    CHECK(std::memcmp(g_out, kGolden, sizeof kGolden) == 0);
    nnh::shutdown();
}

NNH_TEST(an_unchanged_set_saves_an_empty_blob) {
    restart();
    ActionSet set = make();
    set.set_source(Act::Jump, 0, Source::button(Button::X));
    set.set_source(Act::Jump, 0, Source::button(Button::A)); // back to the default
    const nnh::Result<std::size_t> size = set.save_to(g_out, sizeof g_out);
    CHECK(size.ok() && size.value() == fmt::kHeaderSize && g_out[6] == 0);
    nnh::shutdown();
}

NNH_TEST(a_composite_with_two_changed_parts_is_saved_whole) {
    restart();
    ActionSet set = make();
    set.set_part(Act::Move, 1, Part::Up, Button::X);
    set.set_part(Act::Move, 1, Part::Down, Button::Y);
    const nnh::Result<std::size_t> size = set.save_to(g_out, sizeof g_out);
    CHECK(size.ok() && size.value() == fmt::kHeaderSize + fmt::kEntrySize);
    const fmt::Entry e = fmt::decode_entry(g_out + fmt::kHeaderSize);
    CHECK(e.part == 0 && e.kind == static_cast<std::uint8_t>(Source::Kind::Buttons4) && e.slot == 1);
    CHECK(e.data[0] == 2 && e.data[1] == 3 && e.data[2] == 12 && e.data[3] == 13);
    nnh::shutdown();
}

NNH_TEST(a_cleared_slot_is_saved_as_a_none_entry) {
    restart();
    ActionSet set = make();
    set.clear(Act::Jump, 1);
    CHECK(set.save_to(g_out, sizeof g_out).value() == fmt::kHeaderSize + fmt::kEntrySize);
    const fmt::Entry e = fmt::decode_entry(g_out + fmt::kHeaderSize);
    CHECK(e.slot == 1 && e.kind == 0 && e.data[0] == 0);
    nnh::shutdown();
}

NNH_TEST(saving_into_a_short_buffer_fails_and_writes_nothing_past_it) {
    restart();
    ActionSet set = make();
    set.set_source(Act::Jump, 0, Source::button(Button::X));
    std::uint8_t small[20];
    std::memset(small, 0xAA, sizeof small);
    const nnh::Result<std::size_t> r = set.save_to(small, 16);
    CHECK(!r.ok() && r.error().code == nnh::ErrorCode::LimitReached);
    CHECK(small[16] == 0xAA);
    nnh::shutdown();
}

NNH_TEST(validation_accepts_the_golden_blob) {
    int count = 0;
    std::uint32_t version = 0;
    CHECK(fmt::validate(kGolden, sizeof kGolden, count, version) == fmt::Check::Ok);
    CHECK(count == 3 && version == 7);
}

NNH_TEST(validation_refuses_damaged_blobs) {
    int count = 0;
    std::uint32_t version = 0;
    std::uint8_t b[sizeof kGolden];
    const auto with = [&](std::size_t at, std::uint8_t value) {
        std::memcpy(b, kGolden, sizeof b);
        b[at] = value;
        return fmt::validate(b, sizeof b, count, version);
    };
    CHECK(fmt::validate(kGolden, 15, count, version) == fmt::Check::Corrupted);              // short
    CHECK(fmt::validate(kGolden, sizeof kGolden - 1, count, version) == fmt::Check::Corrupted); // truncated
    CHECK(with(0, 'X') == fmt::Check::Corrupted);  // magic
    CHECK(with(4, 2) == fmt::Check::Unsupported);  // newer format
    CHECK(with(4, 0) == fmt::Check::Corrupted);    // no such format
    CHECK(with(5, 1) == fmt::Check::Corrupted);    // reserved
    CHECK(with(6, 4) == fmt::Check::Corrupted);    // count vs size
    CHECK(with(20, 0x55) == fmt::Check::Corrupted); // an entry byte: CRC
    CHECK(with(12, 0) == fmt::Check::Corrupted);   // the CRC itself
    CHECK(with(8, 9) == fmt::Check::Ok);           // the game version is not checked here
}

NNH_TEST(validation_refuses_more_than_256_entries) {
    static std::uint8_t big[fmt::kHeaderSize + fmt::kEntrySize * 257] = {};
    fmt::finish_blob(big, 257, 0);
    int count = 0;
    std::uint32_t version = 0;
    CHECK(fmt::validate(big, sizeof big, count, version) == fmt::Check::Corrupted);
    fmt::finish_blob(big, 256, 0);
    CHECK(fmt::validate(big, sizeof big - fmt::kEntrySize, count, version) == fmt::Check::Ok);
}

NNH_TEST(entries_sort_by_hash_slot_and_part) {
    std::uint8_t e[3 * fmt::kEntrySize];
    fmt::encode_entry({.hash = 5, .slot = 1, .part = 0}, e);
    fmt::encode_entry({.hash = 5, .slot = 0, .part = 4}, e + 12);
    fmt::encode_entry({.hash = 2, .slot = 3, .part = 0}, e + 24);
    fmt::sort_entries(e, 3);
    CHECK(fmt::decode_entry(e).hash == 2);
    CHECK(fmt::decode_entry(e + 12).slot == 0 && fmt::decode_entry(e + 24).slot == 1);
}
