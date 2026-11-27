// Animators meet a reloaded library (spec 031 T5.1, FR-8): the clip of the same name goes on at its frame (clamped into
// a shorter clip); a clip that no longer exists switches to the first clip with one warning; a changed state machine
// restarts at state 0 with a warning; an animator that missed two reloads never reads the freed block.

#include "sprite_reload_fixture.hpp"

namespace srt = sprite_reload_test;

NNH_TEST(animator_rebind_keeps_the_clip_of_the_same_name_at_its_frame) {
    RELOAD_DEV_ONLY();
    srt::start(srt::version1());
    {
        nnh::SpriteLibrary lib = std::move(nnh::load_sprite_library("art/hero").value());
        nnh::SpriteAnimator a{lib};
        a.play("walk");
        for (int i = 0; i < 9; ++i) a.update(); // frame 2 of walk, 1 update in
        CHECK(a.position() == 2);
        const srt::File v2 = srt::version2();
        srt::reload(v2, 1);
        CHECK(std::strcmp(a.clip_name(), "walk") == 0 && a.clip().value == 2); // "jump" now comes first
        CHECK(a.position() == 2);
        for (int i = 0; i < 3; ++i) a.update();
        CHECK(a.position() == 3); // the new 4th frame
    }
    nnh::shutdown();
}

NNH_TEST(animator_rebind_clamps_into_a_shorter_clip) {
    RELOAD_DEV_ONLY();
    srt::start(srt::version2());
    {
        nnh::SpriteLibrary lib = std::move(nnh::load_sprite_library("art/hero").value());
        nnh::SpriteAnimator a{lib};
        a.play("walk");
        for (int i = 0; i < 13; ++i) a.update();
        CHECK(a.position() == 3);
        const srt::File v1 = srt::version1();
        srt::reload(v1, 1);
        CHECK(a.position() == 2 && std::strcmp(a.clip_name(), "walk") == 0);
    }
    nnh::shutdown();
}

NNH_TEST(animator_rebind_a_missing_clip_plays_the_first_with_a_warning) {
    RELOAD_DEV_ONLY();
    srt::start(srt::version1());
    {
        nnh::SpriteLibrary lib = std::move(nnh::load_sprite_library("art/hero").value());
        nnh::SpriteAnimator a{lib};
        a.play("walk");
        a.update();
        reload_test::Lines warned("no longer exists");
        const srt::File small = srt::without_walk();
        srt::reload(small, 1);
        CHECK(std::strcmp(a.clip_name(), "idle") == 0 && a.position() == 0);
        CHECK(warned.count == 1);
        CHECK(std::strstr(warned.last, "clip \"walk\" no longer exists in art/hero: playing \"idle\"") != nullptr);
        a.update();
        CHECK(warned.count == 1);
    }
    nnh::shutdown();
}

NNH_TEST(animator_rebind_a_changed_state_machine_restarts_at_state_0) {
    RELOAD_DEV_ONLY();
    srt::start(srt::version1(true));
    {
        nnh::SpriteLibrary lib = std::move(nnh::load_sprite_library("art/hero").value());
        nnh::SpriteAnimator same{lib, nnh::SpriteStateId{1}};
        nnh::SpriteAnimator changed{lib, nnh::SpriteStateId{1}};
        same.update();
        changed.update();
        const srt::File v2 = srt::version2(true); // the same states and rule, clips moved: the state goes on
        srt::reload(v2, 1);
        CHECK(same.state().value == 1 && std::strcmp(same.clip_name(), "walk") == 0);
        reload_test::Lines warned("state machine");
        const srt::File other = srt::version2(true, true);
        srt::reload(other, 2);
        CHECK(changed.state().value == 0 && std::strcmp(changed.clip_name(), "idle") == 0);
        CHECK(warned.count == 1 && std::strstr(warned.last, "art/hero") != nullptr);
    }
    nnh::shutdown();
}

NNH_TEST(animator_rebind_survives_address_reuse_over_many_reloads) {
    RELOAD_DEV_ONLY();
    srt::start(srt::version1());
    {
        nnh::SpriteLibrary lib = std::move(nnh::load_sprite_library("art/hero").value());
        const srt::File v2 = srt::version2(), small = srt::without_walk();
        srt::reload(v2, 1);
        nnh::SpriteAnimator a{lib};
        a.play("walk"); // clip 2 of v2
        CHECK(a.clip().value == 2);
        // 13 more reloads the animator doesn't see: same-size blocks come and go, addresses come back (review S4).
        for (std::uint16_t id = 2; id <= 14; ++id) srt::reload(id % 2 == 0 ? small : v2, id);
        CHECK(nnh::assets::reload_count() == 14);
        // The last one is v2 (3 clips): whatever the address, the animator is in a valid clip of the current block.
        CHECK(a.clip().value < lib.clip_count());
        CHECK(a.clip_name()[0] != '\0');
        srt::reload(small, 15); // 1 clip now
        CHECK(a.clip().value == 0 && std::strcmp(a.clip_name(), "idle") == 0);
    }
    nnh::shutdown();
}

NNH_TEST(animator_rebind_after_two_reloads_never_reads_the_freed_block) {
    RELOAD_DEV_ONLY();
    srt::start(srt::version1());
    {
        nnh::SpriteLibrary lib = std::move(nnh::load_sprite_library("art/hero").value());
        nnh::SpriteAnimator early{lib};
        early.play("walk");
        const srt::File v2 = srt::version2(), v1 = srt::version1(), v2b = srt::version2();
        srt::reload(v2, 1);
        nnh::SpriteAnimator middle{lib}; // bound to the identity block, then rebinds onto v2
        middle.play("walk");
        CHECK(middle.clip().value == 2);
        srt::reload(v1, 2);
        srt::reload(v2b, 3); // v2 (middle's block) is freed now
        CHECK(middle.clip().value == 0 && std::strcmp(middle.clip_name(), "jump") == 0); // can't know its name: the first
        CHECK(std::strcmp(early.clip_name(), "walk") == 0); // early was on the identity block: still readable
    }
    nnh::shutdown();
}
