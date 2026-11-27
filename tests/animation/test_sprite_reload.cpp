// The sprite library kind of hot reload (spec 031 T5.1, FR-7): the game's SpriteLibrary answers from the reloaded
// content (a larger library too), the block before the previous one is freed at the next reload, destroying the library
// frees every block, and a damaged file refuses the batch.

#include "nnh/memory.hpp"
#include "sprite_reload_fixture.hpp"

namespace srt = sprite_reload_test;

namespace {

std::size_t animation_bytes() { return nnh::mem::tag_stats(nnh::detail::animation_memory_tag()).bytes; }

} // namespace

NNH_TEST(sprite_reload_queries_answer_from_the_new_library) {
    RELOAD_DEV_ONLY();
    srt::start(srt::version1());
    {
        nnh::SpriteLibrary lib = std::move(nnh::load_sprite_library("art/hero").value());
        CHECK(lib.clip_count() == 2 && lib.frame_count() == 4);
        const srt::File bigger = srt::version2();
        srt::reload(bigger, 1);
        CHECK(nnh::assets::reload_count() == 1);
        CHECK(lib.clip_count() == 3 && lib.frame_count() == 6);
        CHECK(lib.find_clip("jump").value == 0 && lib.clip_length(lib.find_clip("walk")) == 4);
        CHECK(lib.memory_used() > 0);
    }
    nnh::shutdown();
}

NNH_TEST(sprite_reload_frees_old_blocks_and_everything_with_the_library) {
    RELOAD_DEV_ONLY();
    srt::start(srt::version1());
    const std::size_t before = animation_bytes();
    {
        nnh::SpriteLibrary lib = std::move(nnh::load_sprite_library("art/hero").value());
        const std::size_t loaded = animation_bytes();
        const srt::File v2 = srt::version2(), v1 = srt::version1(), v2b = srt::version2();
        srt::reload(v2, 1);
        srt::reload(v1, 2);
        const std::size_t two = animation_bytes(); // identity + current + previous
        CHECK(two > loaded);
        srt::reload(v2b, 3);
        CHECK(nnh::assets::reload_count() == 3);
        CHECK(animation_bytes() == two); // v2b came, the v2 block two reloads back went (the same size)
        CHECK(lib.clip_count() == 3);
    }
    CHECK(animation_bytes() <= before + 64 * sizeof(nnh::detail::SpriteReloadEntry)); // only the table is left
    nnh::shutdown();
}

NNH_TEST(sprite_reload_a_damaged_file_keeps_the_old_library) {
    RELOAD_DEV_ONLY();
    srt::start(srt::version1());
    {
        nnh::SpriteLibrary lib = std::move(nnh::load_sprite_library("art/hero").value());
        srt::File bad = srt::version2();
        bad[bad.size() - 3] ^= 0x55; // the CRC no longer matches
        srt::reload(bad, 1);
        CHECK(nnh::assets::reload_count() == 0 && reload_test::records().refused == 1);
        CHECK(lib.clip_count() == 2);
    }
    nnh::shutdown();
}

NNH_TEST(sprite_reload_a_library_moved_after_loading_still_reloads) {
    RELOAD_DEV_ONLY();
    srt::start(srt::version1());
    {
        nnh::SpriteLibrary first = std::move(nnh::load_sprite_library("art/hero").value());
        nnh::SpriteLibrary moved = std::move(first); // the block (the table's identity) moves with it
        nnh::SpriteAnimator a{moved};
        a.play("walk");
        const srt::File v2 = srt::version2();
        srt::reload(v2, 1);
        CHECK(moved.clip_count() == 3 && !first);
        CHECK(std::strcmp(a.clip_name(), "walk") == 0);
    }
    CHECK(animation_bytes() <= 64 * sizeof(nnh::detail::SpriteReloadEntry) + 64); // everything freed but the table
    nnh::shutdown();
}
