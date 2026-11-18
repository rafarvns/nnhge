// The built-in font's texture is created by nnh::init() for every game (spec 005 FR-1, FR-7, FR-8).

#include "../gfx/gfx_fixture.hpp"
#include "nnh/memory.hpp"
#include "text/font_texture.hpp"

namespace fake = nnh::platform_fake;
using gfx_test::load;
using gfx_test::restart;

NNH_TEST(init_creates_the_font_texture) {
    restart();
    CHECK(fake::state().live_textures == 1);
    CHECK(fake::state().rgba_textures_created == 1);
    CHECK(nnh::detail::font_texture_ready());
    nnh::shutdown();
    CHECK(fake::state().live_textures == 0);
}

NNH_TEST(the_font_texture_is_counted_under_its_own_memory_tag) {
    restart();
    CHECK(nnh::mem::tag_stats(nnh::detail::font_memory_tag()).bytes == 128u * 128u * 4u);
    nnh::shutdown();
}

NNH_TEST(the_font_does_not_count_against_the_games_textures) {
    restart({.gfx = {.max_textures = 4}});
    CHECK(nnh::gfx::stats().textures == 0 && nnh::gfx::stats().texture_bytes == 0);
    CHECK(nnh::gfx::stats().max_textures == 4);
    nnh::Texture t[4];
    for (nnh::Texture& each : t) each = load("tile"); // all 4 still fit
    CHECK(nnh::gfx::stats().textures == 4);
    nnh::shutdown();
}
