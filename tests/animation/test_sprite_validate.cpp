// The `.nnhsprite` validator (T6.1; FR-16, constitution X): a code-built library written as a file passes; one
// corruption per rule is refused with the right problem.

#include "animation/sprite_validate.hpp"
#include "library_fixture.hpp"
#include "sprite_file.hpp"

using namespace nnh::detail;
using namespace nnh::detail::spritefmt;
using anim_test::get;
using anim_test::put;
using File = std::vector<std::uint8_t>;

namespace {

File good() {
    return anim_test::sprite_file(anim_test::rich_library());
}

SpriteProblem problem_of(File f, bool reseal = true) {
    if (reseal) anim_test::reseal(f);
    return validate_sprite_library(f.data(), f.size()).problem;
}

/// Edits the header, reseals, validates.
template <typename Edit>
SpriteProblem header_edit(Edit edit) {
    File f = good();
    Header h = anim_test::header_of(f);
    edit(h);
    anim_test::set_header(f, h);
    return problem_of(f);
}

template <typename T, typename Edit>
SpriteProblem record_edit(int section, int index, Edit edit) {
    File f = good();
    T r = get<T>(f, section, index);
    edit(r);
    put(f, section, index, r);
    return problem_of(f);
}

constexpr SpriteProblem kDamaged = SpriteProblem::Damaged;

} // namespace

NNH_TEST(a_code_built_library_written_as_a_file_is_valid) {
    const File f = good();
    CHECK(validate_sprite_library(f.data(), f.size()).ok());
    const File plain = anim_test::sprite_file(anim_test::strip(3));
    CHECK(validate_sprite_library(plain.data(), plain.size()).ok());
}

NNH_TEST(the_header_is_checked_first) {
    File f = good();
    CHECK(validate_sprite_library(nullptr, 0).problem == kDamaged);
    CHECK(validate_sprite_library(f.data(), 63).problem == kDamaged);
    CHECK(validate_sprite_library(f.data(), f.size() - 1).problem == kDamaged); // truncated
    f[200] ^= 0x40;
    CHECK(problem_of(f, false) == kDamaged); // CRC
    CHECK(header_edit([](Header& h) { h.magic[3] = 'M'; }) == kDamaged);
    CHECK(header_edit([](Header& h) { h.version = 2; }) == SpriteProblem::NewerVersion);
    CHECK(header_edit([](Header& h) { h.version = 0; }) == kDamaged);
    CHECK(header_edit([](Header& h) { h.serial = 77; }) == SpriteProblem::None); // memory only
    CHECK(header_edit([](Header& h) { h.cap[kStates] = 40; }) == SpriteProblem::OverLimit);
    CHECK(header_edit([](Header& h) { h.cap[kFrames] = 0; }) == kDamaged);
    CHECK(header_edit([](Header& h) { h.offset[1] += 4; }) == kDamaged);
    CHECK(header_edit([](Header& h) { h.count[kMarkers] = 7; }) == kDamaged);
    CHECK(header_edit([](Header& h) { h.pivot = 3; }) == kDamaged);
    CHECK(header_edit([](Header& h) { h.reserved[1] = 1; }) == kDamaged);
}

NNH_TEST(frames_are_checked) {
    CHECK(record_edit<FrameEntry>(kFrames, 1, [](FrameEntry& e) { e.w = 0; }) == kDamaged);
    CHECK(record_edit<FrameEntry>(kFrames, 1, [](FrameEntry& e) { e.y = -1; }) == kDamaged);
    CHECK(record_edit<FrameEntry>(kFrames, 1, [](FrameEntry& e) { e.x = 32760; }) == kDamaged);
    CHECK(record_edit<FrameEntry>(kFrames, 5, [](FrameEntry& e) { e.offset_x = -2; }) == kDamaged);
    CHECK(record_edit<FrameEntry>(kFrames, 5, [](FrameEntry& e) { e.pivot_x = -300; }) == SpriteProblem::None);
}

NNH_TEST(clip_frames_are_checked) {
    CHECK(record_edit<ClipFrameEntry>(kClipFrames, 3, [](ClipFrameEntry& e) { e.duration = 0; }) == kDamaged);
    CHECK(record_edit<ClipFrameEntry>(kClipFrames, 3, [](ClipFrameEntry& e) { e.duration = kMaxTicks + 1; }) ==
          kDamaged);
    CHECK(record_edit<ClipFrameEntry>(kClipFrames, 3, [](ClipFrameEntry& e) { e.frame = 6; }) == kDamaged);
    CHECK(record_edit<ClipFrameEntry>(kClipFrames, 3, [](ClipFrameEntry& e) { ++e.markers; }) == kDamaged);
    CHECK(record_edit<ClipFrameEntry>(kClipFrames, 3, [](ClipFrameEntry& e) { ++e.first_marker; }) == kDamaged);
    CHECK(record_edit<ClipFrameEntry>(kClipFrames, 3, [](ClipFrameEntry& e) { e.pad[2] = 1; }) == kDamaged);
}

NNH_TEST(clips_are_checked) {
    CHECK(record_edit<ClipEntry>(kClips, 1, [](ClipEntry& c) { c.name[0] = 'R'; }) == kDamaged); // hash
    CHECK(record_edit<ClipEntry>(kClips, 1, [](ClipEntry& c) { c.name[10] = 'x'; }) == kDamaged); // padding
    CHECK(record_edit<ClipEntry>(kClips, 1, [](ClipEntry& c) { c.name[0] = '\0'; c.hash = 0x811C9DC5u; }) ==
          kDamaged);
    CHECK(record_edit<ClipEntry>(kClips, 1, [](ClipEntry& c) { c.first = 3; }) == kDamaged);
    CHECK(record_edit<ClipEntry>(kClips, 4, [](ClipEntry& c) { c.length = 5; }) == kDamaged);
    CHECK(record_edit<ClipEntry>(kClips, 1, [](ClipEntry& c) { c.mode = 4; }) == kDamaged);
    CHECK(record_edit<ClipEntry>(kClips, 1, [](ClipEntry& c) { c.direction = 2; }) == kDamaged);
    CHECK(record_edit<ClipEntry>(kClips, 0, [](ClipEntry& c) { c.count = 1; }) == kDamaged);  // a loop with a count
    CHECK(record_edit<ClipEntry>(kClips, 2, [](ClipEntry& c) { c.count = 2; }) == kDamaged);  // once ≠ 1
    CHECK(record_edit<ClipEntry>(kClips, 3, [](ClipEntry& c) { c.count = 0; }) == kDamaged);  // repeat 0
    CHECK(record_edit<ClipEntry>(kClips, 1, [](ClipEntry& c) { c.count = 3; }) == kDamaged);  // end mismatch
    CHECK(record_edit<ClipEntry>(kClips, 3, [](ClipEntry& c) { ++c.end_step; }) == kDamaged);
    CHECK(record_edit<ClipEntry>(kClips, 3, [](ClipEntry& c) { ++c.end_cycle; }) == kDamaged);
    CHECK(record_edit<ClipEntry>(kClips, 2, [](ClipEntry& c) { ++c.cycle_ticks; }) == kDamaged);
    File dup = good();
    ClipEntry second = get<ClipEntry>(dup, kClips, 1);
    const ClipEntry first = get<ClipEntry>(dup, kClips, 0);
    std::memcpy(second.name, first.name, sizeof second.name);
    second.hash = first.hash;
    put(dup, kClips, 1, second);
    CHECK(problem_of(dup) == kDamaged);
}

NNH_TEST(states_and_rules_are_checked) {
    CHECK(record_edit<StateEntry>(kStates, 2, [](StateEntry& s) { s.clip = 5; }) == kDamaged);
    CHECK(record_edit<StateEntry>(kStates, 2, [](StateEntry& s) { s.next = 3; }) == kDamaged);
    CHECK(record_edit<StateEntry>(kStates, 2, [](StateEntry& s) { s.pad = 1; }) == kDamaged);
    CHECK(record_edit<StateEntry>(kStates, 2, [](StateEntry& s) { s.hash ^= 1; }) == kDamaged);
    CHECK(record_edit<StateEntry>(kStates, 1, [](StateEntry& s) {
              std::memcpy(s.name, "idle", 5);
              s.hash = nnh::detail::fnv1a32("idle");
          }) == kDamaged); // two states called idle
    CHECK(record_edit<TransitionEntry>(kTransitions, 0, [](TransitionEntry& t) { t.from = 3; }) == kDamaged);
    CHECK(record_edit<TransitionEntry>(kTransitions, 0, [](TransitionEntry& t) { t.to = 9; }) == kDamaged);
    CHECK(record_edit<TransitionEntry>(kTransitions, 0, [](TransitionEntry& t) { t.when = 3; }) == kDamaged);
    CHECK(record_edit<TransitionEntry>(kTransitions, 0, [](TransitionEntry& t) { t.keep_time = 2; }) == kDamaged);
    CHECK(record_edit<TransitionEntry>(kTransitions, 1, [](TransitionEntry& t) { t.from = 2; t.to = kAnyState; }) ==
          kDamaged); // the same states as rule 0
}

NNH_TEST(clips_must_own_their_frames_in_order) {
    // Two clips of equal-duration frames: pointing the second at the first's frames changes no total and no
    // duration, only the run order (mutation survivor, spec 016 T6.1).
    auto made = nnh::SpriteLibrary::create({.frames = 4, .clips = 2, .clip_frames = 4});
    nnh::SpriteLibrary lib = std::move(made.value());
    CHECK(lib.add_grid({.frame_width = 8, .frame_height = 8, .count = 4, .columns = 4}).ok());
    CHECK(lib.add_clip({.name = "a", .first = 0, .count = 2}).ok());
    CHECK(lib.add_clip({.name = "b", .first = 0, .count = 2}).ok());
    File f = anim_test::sprite_file(lib);
    CHECK(validate_sprite_library(f.data(), f.size()).ok());
    ClipEntry b = get<ClipEntry>(f, kClips, 1);
    b.first = 0;
    put(f, kClips, 1, b);
    CHECK(problem_of(f) == kDamaged);
}
