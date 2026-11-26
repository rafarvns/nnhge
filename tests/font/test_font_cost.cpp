// Fonts cost nothing while playing (spec 029 FR-34): after load_font, drawing, measuring and wrapping allocate nothing.

#include "font_load_fixture.hpp"
#include "nnh/font.hpp"
#include "nnh/gfx.hpp"
#include "nnh/memory.hpp"
#include "test.hpp"

using namespace font_test;

NNH_TEST(font_draw_measure_and_wrap_allocate_nothing) {
    start_engine({.max_draws = 4096});
    FontBuilder b = FontBuilder::latin();
    b.kern(U'A', U'V', -1);
    add_font("fonts/a", b);
    auto f = nnh::load_font("fonts/a");
    CHECK(f.ok());
    if (!f) return;
    const nnh::TextOptions o{.font = f.value(), .wrap_width = 90, .max_lines = 3};
    const std::size_t before = nnh::mem::live_allocations();
    nnh::TextCaret carets[64];
    for (int i = 0; i < 2000; ++i) {
        CHECK(nnh::frame());
        nnh::gfx::draw_text(nnh::Screen::Top, {4, 4}, "AVATAR of the north gate opens at dawn", o);
        (void)nnh::gfx::measure_text("AVATAR of the north gate opens at dawn", o);
        (void)nnh::gfx::measure_chars("AVATAR of the north", o, carets, 64);
    }
    CHECK(static_cast<std::size_t>(nnh::mem::live_allocations()) == before);
}
