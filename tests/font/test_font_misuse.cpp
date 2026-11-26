// Font misuse (spec 029 FR-35): loud in Debug/Development, harmless in Release — a destroyed font drawn or measured (the
// built-in font), an outline colour on a font without outlines (ignored, said once), a scale outside 1..8 (clamped).

#include "crash/crash_internal.hpp"
#include "font_load_fixture.hpp"
#include "nnh/build.hpp"
#include "nnh/font.hpp"
#include "nnh/gfx.hpp"
#include "test.hpp"

using namespace font_test;
namespace fake = nnh::platform_fake;

namespace {

nnh::FontRef g_stale;

bool crashes(void (*fn)()) {
    nnh::detail::crash_install();
    return fake::run_expecting_crash(fn);
}

} // namespace

NNH_TEST(font_misuse_a_destroyed_font_asserts_or_draws_the_builtin) {
    start_engine();
    add_font("fonts/a", FontBuilder::latin());
    {
        auto f = nnh::load_font("fonts/a");
        CHECK(f.ok());
        g_stale = f ? f.value().ref() : nnh::FontRef{};
    } // the last owner is gone
    CHECK(nnh::frame());
    const bool crashed = crashes([] { nnh::gfx::draw_text(nnh::Screen::Top, {0, 0}, "ab", {.font = g_stale}); });
    CHECK(crashed == nnh::build::kAssertsEnabled);
    if (!crashed) { // Release: the built-in font's two cells
        CHECK(nnh::gfx::measure_text("ab", {.font = g_stale}).x == 16);
    }
    nnh::frame();
}

NNH_TEST(font_misuse_an_outline_colour_without_outlines_is_ignored_once) {
    start_engine();
    add_font("fonts/a", FontBuilder::latin());
    auto f = nnh::load_font("fonts/a");
    CHECK(f.ok());
    if (!f) return;
    LogLines warnings(nnh::LogLevel::Warn);
    CHECK(nnh::frame());
    for (int i = 0; i < 3; ++i) {
        nnh::gfx::draw_text(nnh::Screen::Top, {0, 0}, "A", {.font = f.value(), .outline_color = nnh::colors::black});
    }
    CHECK(nnh::frame());
    CHECK(fake::state().last_frame_item_count == 3); // the glyphs, no outline
    CHECK(warnings.count("outline_color ignored") == (nnh::build::kDevToolsEnabled ? 1 : 0));
}

NNH_TEST(font_misuse_a_scale_outside_1_to_8_is_clamped) {
    start_engine();
    add_font("fonts/a", FontBuilder::latin());
    auto f = nnh::load_font("fonts/a");
    CHECK(f.ok());
    if (!f) return;
    CHECK(nnh::gfx::measure_text("A", {.scale = 0, .font = f.value()}).x == 8);
    CHECK(nnh::gfx::measure_text("A", {.scale = 50, .font = f.value()}).x == 64);
}
