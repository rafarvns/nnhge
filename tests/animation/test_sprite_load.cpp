// Loading `.nnhsprite` assets (T6.2; FR-16, FR-18): through the fake platform's assets, with extra capacities for
// markers, states and rules added in code; errors for damaged, newer and over-limit files; a loaded library plays
// exactly like the code-built one it was written from.

#include <utility>

#include "library_fixture.hpp"
#include "nnh/memory.hpp"
#include "sprite_file.hpp"

namespace fake = nnh::platform_fake;
using File = std::vector<std::uint8_t>;
using nnh::SpriteAnimator;
using nnh::SpriteLibrary;

namespace {

File g_hero, g_bad;

/// Restarts the engine with art/hero.nnhsprite (rich_library) and art/bad.nnhsprite (`bad`).
void start(File bad = {}) {
    nnh::shutdown();
    fake::reset();
    g_hero = anim_test::sprite_file(anim_test::rich_library());
    g_bad = std::move(bad);
    static const char index[] = "art/bad.nnhsprite\nart/hero.nnhsprite\n";
    fake::add_asset(".nnh_assets", index, sizeof index - 1);
    fake::add_asset("art/hero.nnhsprite", g_hero.data(), g_hero.size());
    fake::add_asset("art/bad.nnhsprite", g_bad.data(), g_bad.size());
    CHECK(nnh::init());
}

/// A stream of every frame and notice over `frames` updates of `a` playing the state machine from state 0.
std::uint32_t play_hash(const SpriteLibrary& lib, int frames) {
    SpriteAnimator a{lib, nnh::SpriteStateId{0}};
    std::uint32_t h = 2166136261u;
    for (int f = 0; f < frames; ++f) {
        a.request(nnh::SpriteStateId{static_cast<std::uint8_t>((f / 11) % 3)});
        for (const nnh::SpriteNotice& n : a.update()) {
            h = (h ^ (static_cast<std::uint32_t>(n.kind) * 131 + n.marker + static_cast<std::uint32_t>(n.value))) *
                16777619u;
        }
        h = (h ^ static_cast<std::uint32_t>(a.frame() * 7 + a.position())) * 16777619u;
    }
    return h;
}

} // namespace

NNH_TEST(a_loaded_library_is_sized_to_its_contents) {
    start();
    {
        auto lib = nnh::load_sprite_library("art/hero");
        CHECK(lib.ok());
        CHECK(lib.value().frame_count() == 6 && lib.value().clip_count() == 5 && lib.value().state_count() == 3);
        CHECK(lib.value().find_clip("run").value == 1);
        CHECK(lib.value().memory_used() < g_hero.size()); // the code-built one had spare capacity
        CHECK(!lib.value().add_frame({.region = {0, 0, 1, 1}}).ok()); // no extras asked for: full
    }
    nnh::shutdown();
}

NNH_TEST(a_loaded_library_plays_exactly_like_the_one_it_was_written_from) {
    start();
    {
        const SpriteLibrary built = anim_test::rich_library();
        auto loaded = nnh::load_sprite_library("art/hero");
        CHECK(loaded.ok());
        CHECK(play_hash(built, 400) == play_hash(loaded.value(), 400));
    }
    nnh::shutdown();
}

NNH_TEST(extras_let_code_add_markers_states_and_rules_to_a_loaded_library) {
    start();
    {
        auto loaded = nnh::load_sprite_library("art/hero", {.extra_markers = 1, .extra_states = 1,
                                                            .extra_transitions = 1});
        CHECK(loaded.ok());
        SpriteLibrary lib = std::move(loaded.value());
        const nnh::SpriteClipId spin = lib.find_clip("spin");
        CHECK(lib.add_marker(spin, 0, "whoosh").ok());
        CHECK(!lib.add_marker(spin, 1, "more").ok());
        const nnh::SpriteStateId s = lib.add_state("spin", spin).value();
        CHECK(lib.add_transition({.from = s, .to = lib.find_state("idle"), .when = nnh::SpriteSwitch::AtEnd}).ok());
        SpriteAnimator a{lib, s};
        const nnh::SpriteNotices n = a.update();
        CHECK(n.count == 1 && n.data[0].marker == nnh::sprite_name("whoosh"));
        // The run clip's markers (before spin in the table) still fire where they were.
        a.play(lib.find_clip("run"));
        int steps = 0;
        for (int i = 0; i < 36; ++i) steps += anim_test::count_kind(a.update(), nnh::SpriteNoticeKind::Marker);
        CHECK(steps == 4);
    }
    nnh::shutdown();
}

NNH_TEST(damaged_newer_and_over_limit_files_are_refused) {
    File damaged = anim_test::sprite_file(anim_test::rich_library());
    damaged[100] ^= 1;
    start(damaged);
    auto bad = nnh::load_sprite_library("art/bad");
    CHECK(!bad.ok() && bad.error().code == nnh::ErrorCode::Corrupted);
    CHECK(nnh::load_sprite_library("art/missing").error().code == nnh::ErrorCode::NotFound);
    CHECK(nnh::load_sprite_library(nullptr).error().code == nnh::ErrorCode::InvalidArgument);
    auto too_many = nnh::load_sprite_library("art/hero", {.extra_states = 30});
    CHECK(!too_many.ok() && too_many.error().code == nnh::ErrorCode::LimitReached);
    CHECK(nnh::load_sprite_library("art/hero", {.extra_frames = -1}).error().code ==
          nnh::ErrorCode::InvalidArgument);

    File newer = anim_test::sprite_file(anim_test::rich_library());
    nnh::detail::spritefmt::Header h = anim_test::header_of(newer);
    h.version = 9;
    anim_test::set_header(newer, h);
    anim_test::reseal(newer);
    start(newer);
    CHECK(nnh::load_sprite_library("art/bad").error().code == nnh::ErrorCode::Unsupported);
    nnh::shutdown();
}

NNH_TEST(loading_leaves_only_the_librarys_block) {
    start();
    const int before = nnh::mem::live_allocations();
    {
        auto lib = nnh::load_sprite_library("art/hero");
        CHECK(nnh::mem::live_allocations() == before + 1); // the asset's buffer is already freed
    }
    CHECK(nnh::mem::live_allocations() == before);
    nnh::shutdown();
}
