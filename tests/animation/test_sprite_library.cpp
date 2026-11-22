// SpriteLibrary storage (T1.2): one block under the `animation` tag, required capacities, move, find by name.

#include <utility>

#include "library_fixture.hpp"
#include "nnh/memory.hpp"

using nnh::SpriteLibrary;

NNH_TEST(a_library_takes_one_block_under_the_animation_tag) {
    const nnh::MemStats before = nnh::mem::tag_stats(nnh::detail::animation_memory_tag());
    {
        auto lib = SpriteLibrary::create({.frames = 40, .clips = 8, .clip_frames = 64, .markers = 6, .states = 6,
                                          .transitions = 12});
        CHECK(lib.ok());
        const nnh::MemStats during = nnh::mem::tag_stats(nnh::detail::animation_memory_tag());
        CHECK(during.count == before.count + 1);
        CHECK(during.bytes == before.bytes + 2224);
        CHECK(lib.value().memory_used() == 2224);
    }
    const nnh::MemStats after = nnh::mem::tag_stats(nnh::detail::animation_memory_tag());
    CHECK(after.count == before.count && after.bytes == before.bytes);
}

NNH_TEST(library_capacities_are_required_and_ceilinged) {
    CHECK(!SpriteLibrary::create({}).ok()); // frames = 0: no hidden default
    auto huge = SpriteLibrary::create({.frames = 5000});
    CHECK(!huge.ok() && huge.error().code == nnh::ErrorCode::InvalidArgument);
    CHECK(!SpriteLibrary::create({.frames = 1, .clips = 300}).ok());
    CHECK(!SpriteLibrary::create({.frames = 1, .markers = -1}).ok());
    CHECK(!SpriteLibrary::create({.frames = 1, .pivot = static_cast<nnh::SpritePivot>(7)}).ok());
    CHECK(SpriteLibrary::create({.frames = 1}).ok());
}

NNH_TEST(moving_a_library_keeps_its_block) {
    SpriteLibrary a = anim_test::strip(4);
    const nnh::SpriteClipId run = anim_test::clip(a, "run", 4, 6);
    nnh::SpriteAnimator anim{a};
    anim.play(run);
    SpriteLibrary b = std::move(a);
    CHECK(!a && b);
    CHECK(b.clip_count() == 1 && a.clip_count() == 0);
    anim.update();
    CHECK(anim.frame() == 0); // still valid: the block moved with the library
    SpriteLibrary c;
    c = std::move(b);
    CHECK(c.frame_count() == 4 && !b);
}

NNH_TEST(an_empty_library_answers_nothing_and_refuses_adds) {
    SpriteLibrary lib;
    CHECK(lib.frame_count() == 0 && lib.clip_count() == 0 && lib.state_count() == 0 && lib.memory_used() == 0);
    CHECK(!lib.find_clip("run").valid() && !lib.find_state("idle").valid());
    CHECK(std::strcmp(lib.clip_name({0}), "") == 0);
    CHECK(!lib.add_grid({.frame_width = 8, .frame_height = 8, .count = 1, .columns = 1}).ok());
    CHECK(!lib.add_frame({.region = {0, 0, 8, 8}}).ok());
    CHECK(!lib.add_clip({.name = "x", .count = 1}).ok());
}

NNH_TEST(clips_are_found_by_name_and_played_by_name) {
    SpriteLibrary lib = anim_test::strip(8);
    anim_test::clip(lib, "idle", 1, 6);
    const nnh::SpriteClipId run = anim_test::clip(lib, "run", 4, 6);
    CHECK(lib.find_clip("run") == run);
    CHECK(!lib.find_clip("walk").valid());
    nnh::SpriteAnimator a{lib};
    a.play("run");
    CHECK(a.clip() == run && std::strcmp(a.clip_name(), "run") == 0);
}
