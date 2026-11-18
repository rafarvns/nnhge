// Loading and releasing textures (nnh/texture.hpp, spec 004 FR-2, FR-3, FR-11).

#include <cstring>
#include <utility>

#include "gfx_fixture.hpp"
#include "nnh/memory.hpp"

namespace fake = nnh::platform_fake;
using gfx_test::load;
using gfx_test::restart;

namespace {

bool message_has(const nnh::Error& e, const char* text) { return std::strstr(e.message, text) != nullptr; }

std::size_t linear_bytes() { return nnh::mem::region_stats(nnh::MemRegion::Linear).bytes; }

} // namespace

NNH_TEST(a_texture_loads_by_image_name) {
    restart();
    nnh::Texture hero = load("hero");
    CHECK(hero.valid() && hero.width() == 64 && hero.height() == 32);
    CHECK(fake::state().live_textures == 1);
    CHECK(nnh::gfx::stats().textures == 1);
    nnh::shutdown();
}

NNH_TEST(an_unknown_image_name_is_not_found) {
    restart();
    auto result = nnh::load_texture("nope");
    CHECK(!result.ok());
    CHECK(result.error().code == nnh::ErrorCode::NotFound);
    CHECK(message_has(result.error(), "nope"));
    nnh::shutdown();
}

NNH_TEST(an_image_that_breaks_the_size_rule_is_refused_and_released) {
    restart();
    auto npot = nnh::load_texture("npot");
    CHECK(!npot.ok() && npot.error().code == nnh::ErrorCode::InvalidArgument);
    CHECK(message_has(npot.error(), "npot") && message_has(npot.error(), "100x64"));
    auto small = nnh::load_texture("too_small");
    CHECK(!small.ok());
    CHECK(fake::state().live_textures == 0); // the platform texture was destroyed again
    CHECK(linear_bytes() == 0);
    nnh::shutdown();
}

NNH_TEST(an_image_the_platform_cannot_decode_fails) {
    restart();
    auto result = nnh::load_texture("garbage");
    CHECK(!result.ok() && result.error().code == nnh::ErrorCode::Corrupted);
    CHECK(message_has(result.error(), "garbage"));
    nnh::shutdown();
}

NNH_TEST(a_full_texture_table_refuses_more_textures) {
    restart({.gfx = {.max_textures = 4}});
    nnh::Texture t[4];
    for (nnh::Texture& each : t) each = load("tile");
    auto extra = nnh::load_texture("tile");
    CHECK(!extra.ok() && extra.error().code == nnh::ErrorCode::LimitReached);
    CHECK(fake::state().live_textures == 4);
    CHECK(nnh::gfx::stats().max_textures == 4);
    nnh::shutdown();
}

NNH_TEST(texture_memory_is_counted_in_linear_memory_until_released) {
    restart();
    {
        nnh::Texture hero = load("hero");
        CHECK(linear_bytes() == 64u * 32u * 4u);
        CHECK(nnh::gfx::stats().texture_bytes == 64u * 32u * 4u);
    } // destroyed outside a frame: released at once
    CHECK(linear_bytes() == 0);
    CHECK(fake::state().live_textures == 0);
    nnh::shutdown();
}

NNH_TEST(a_texture_destroyed_during_a_frame_is_released_after_the_frame) {
    restart();
    nnh::Texture hero = load("hero");
    CHECK(nnh::frame());
    nnh::gfx::draw(nnh::Screen::Top, hero, {0, 0});
    hero.reset();
    CHECK(!hero.valid());
    CHECK(fake::state().live_textures == 1); // the GPU may still need it this frame
    CHECK(nnh::frame());
    CHECK(fake::state().last_frame_item_count == 1); // drawn before it was destroyed: still shown
    CHECK(fake::state().live_textures == 0);
    CHECK(linear_bytes() == 0);
    nnh::shutdown();
}

NNH_TEST(moving_a_texture_moves_ownership) {
    restart();
    nnh::Texture a = load("hero");
    nnh::Texture b = std::move(a);
    CHECK(!a.valid() && b.valid() && b.width() == 64);
    nnh::Texture c = load("tile");
    c = std::move(b); // the tile is released
    CHECK(c.valid() && c.width() == 64);
    CHECK(fake::state().live_textures == 1);
    nnh::shutdown();
}

NNH_TEST(textures_still_alive_at_shutdown_are_released_and_their_handles_go_stale) {
    restart();
    nnh::Texture hero = load("hero");
    nnh::shutdown();
    CHECK(fake::state().live_textures == 0);
    CHECK(!hero.valid());
    restart();
    nnh::Texture tile = load("tile"); // reuses the slot hero had
    CHECK(!hero.valid() && tile.valid());
    hero.reset(); // harmless: must not release the tile
    CHECK(tile.valid() && fake::state().live_textures == 1);
    nnh::shutdown();
    hero.reset(); // harmless after shutdown too
}

NNH_TEST(loading_before_init_fails_cleanly) {
    nnh::shutdown();
    auto result = nnh::load_texture("hero");
    CHECK(!result.ok());
}
