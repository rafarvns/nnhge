// Frame markers in the library (T4.1; FR-8): sorted by clip frame, in the order added, 8 per frame, errors.

#include <cstring>

#include "library_fixture.hpp"

using nnh::SpriteLibrary;
using nnh::sprite_name;

namespace {

/// The markers fired by the first three updates of a clip of 1-frame frames (the entry, then two crossings).
int fired(nnh::SpriteAnimator& a, std::uint32_t* out, int max) {
    int n = 0;
    for (int u = 0; u < 3; ++u) {
        for (const nnh::SpriteNotice& x : a.update()) {
            if (x.kind == nnh::SpriteNoticeKind::Marker && n < max) out[n++] = x.marker;
        }
    }
    return n;
}

} // namespace

NNH_TEST(markers_of_a_frame_fire_in_the_order_they_were_added) {
    SpriteLibrary lib = anim_test::strip(4);
    const nnh::SpriteClipId run = anim_test::clip(lib, "run", 4, 1);
    const nnh::SpriteClipId other = anim_test::clip(lib, "other", 4, 1);
    CHECK(lib.add_marker(run, 2, "b").ok());
    CHECK(lib.add_marker(other, 0, "x").ok()); // a later clip: shifts nothing in "run"
    CHECK(lib.add_marker(run, 0, "a").ok());   // inserted before frame 2's markers
    CHECK(lib.add_marker(run, 2, "c", 7).ok());
    nnh::SpriteAnimator a{lib};
    a.play(run);
    std::uint32_t got[8] = {};
    CHECK(fired(a, got, 8) == 3);
    CHECK(got[0] == sprite_name("a") && got[1] == sprite_name("b") && got[2] == sprite_name("c"));
    a.play(other);
    CHECK(fired(a, got, 8) == 1 && got[0] == sprite_name("x"));
}

NNH_TEST(marker_values_are_reported) {
    SpriteLibrary lib = anim_test::strip(2);
    const nnh::SpriteClipId hit = anim_test::clip(lib, "hit", 2, 1);
    CHECK(lib.add_marker(hit, 1, "hit", -3).ok());
    nnh::SpriteAnimator a{lib};
    a.play(hit);
    const nnh::SpriteNotices n = a.update(); // enters frame 0, then crosses into frame 1
    CHECK(n.count == 1 && n.data[0].value == -3 && n.data[0].position == 1 && n.data[0].clip == hit);
}

NNH_TEST(at_most_eight_markers_per_frame) {
    SpriteLibrary lib = anim_test::strip(2);
    const nnh::SpriteClipId c = anim_test::clip(lib, "c", 2, 1);
    for (int i = 0; i < 8; ++i) CHECK(lib.add_marker(c, 0, "m", i).ok());
    auto ninth = lib.add_marker(c, 0, "m", 8);
    CHECK(!ninth.ok() && ninth.error().code == nnh::ErrorCode::LimitReached);
    CHECK(lib.add_marker(c, 1, "m").ok());
}

NNH_TEST(marker_errors_are_returned) {
    auto made = SpriteLibrary::create({.frames = 2, .clips = 1, .clip_frames = 2, .markers = 1});
    SpriteLibrary lib = std::move(made.value());
    CHECK(lib.add_grid({.frame_width = 8, .frame_height = 8, .count = 2, .columns = 2}).ok());
    const nnh::SpriteClipId c = lib.add_clip({.name = "c", .count = 2}).value();
    CHECK(!lib.add_marker({5}, 0, "m").ok());
    CHECK(!lib.add_marker(c, 2, "m").ok());
    CHECK(!lib.add_marker(c, -1, "m").ok());
    CHECK(!lib.add_marker(c, 0, "").ok());
    CHECK(!lib.add_marker(c, 0, "01234567890123456789012345678901").ok());
    CHECK(lib.add_marker(c, 0, "m").ok());
    auto full = lib.add_marker(c, 1, "n");
    CHECK(!full.ok() && full.error().code == nnh::ErrorCode::LimitReached);
}

NNH_TEST(sprite_name_is_a_constant_expression) {
    constexpr std::uint32_t step = sprite_name("step");
    switch (nnh::detail::fnv1a32("step")) {
    case step: break;
    default: CHECK(false);
    }
}
