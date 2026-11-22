// The library block's layout (T1.1): record sizes, section offsets 4-byte aligned, totals, capacity ranges.

#include "animation/sprite_layout.hpp"
#include "test.hpp"

using namespace nnh::detail;
using namespace nnh::detail::spritefmt;

namespace {

SpriteCapacities caps(int frames, int clips, int clip_frames, int markers, int states, int transitions) {
    SpriteCapacities c;
    const int v[] = {frames, clips, clip_frames, markers, states, transitions};
    for (int s = 0; s < kSectionCount; ++s) c.cap[s] = v[s];
    return c;
}

} // namespace

NNH_TEST(sprite_records_have_their_planned_sizes) {
    CHECK(sizeof(Header) == 64 && sizeof(FrameEntry) == 16 && sizeof(ClipEntry) == 52);
    CHECK(sizeof(ClipFrameEntry) == 12 && sizeof(MarkerEntry) == 8 && sizeof(StateEntry) == 40);
    CHECK(sizeof(TransitionEntry) == 4);
}

NNH_TEST(sprite_sections_follow_the_header_in_order) {
    const SpriteLayout l = sprite_layout(caps(40, 8, 64, 6, 6, 12));
    CHECK(l.offset[kFrames] == 64);
    CHECK(l.offset[kClips] == 64 + 40 * 16);
    CHECK(l.offset[kClipFrames] == l.offset[kClips] + 8 * 52);
    CHECK(l.offset[kMarkers] == l.offset[kClipFrames] + 64 * 12);
    CHECK(l.offset[kStates] == l.offset[kMarkers] + 6 * 8);
    CHECK(l.offset[kTransitions] == l.offset[kStates] + 6 * 40);
    CHECK(l.total == l.offset[kTransitions] + 12 * 4);
    for (std::uint32_t offset : l.offset) CHECK(offset % 4 == 0);
}

NNH_TEST(sprite_layout_of_the_plans_examples) {
    CHECK(sprite_layout(caps(40, 8, 64, 6, 6, 12)).total == 2224); // the example's hero: ≈ 2.2 KB
    CHECK(sprite_layout(caps(4, 4, 5, 2, 4, 0)).total == 572);     // the platformer's player: ≈ 0.6 KB
    CHECK(sprite_layout(caps(4096, 256, 8192, 1024, 32, 128)).total == 187200); // the ceilings: ≈ 187 KB
}

NNH_TEST(sprite_capacities_are_checked_against_the_ceilings) {
    CHECK(check_sprite_capacities(caps(1, 0, 0, 0, 0, 0)) == nullptr);
    CHECK(check_sprite_capacities(caps(4096, 256, 8192, 1024, 32, 128)) == nullptr);
    CHECK(check_sprite_capacities(caps(0, 0, 0, 0, 0, 0)) != nullptr);
    CHECK(check_sprite_capacities(caps(4097, 0, 0, 0, 0, 0)) != nullptr);
    CHECK(check_sprite_capacities(caps(1, 257, 0, 0, 0, 0)) != nullptr);
    CHECK(check_sprite_capacities(caps(1, 0, 8193, 0, 0, 0)) != nullptr);
    CHECK(check_sprite_capacities(caps(1, 0, 0, 1025, 0, 0)) != nullptr);
    CHECK(check_sprite_capacities(caps(1, 0, 0, 0, 33, 0)) != nullptr);
    CHECK(check_sprite_capacities(caps(1, 0, 0, 0, 0, 129)) != nullptr);
    CHECK(check_sprite_capacities(caps(1, -1, 0, 0, 0, 0)) != nullptr);
}
