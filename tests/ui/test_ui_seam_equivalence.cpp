// Spec 028's seam, from cells to pixels (spec 029 FR-14, plan "Spec 028's seam"): part 1 — the shared wrapper through the
// built-in face gives, for Latin-1 text, exactly the lines, bytes, widths (cells × 8 × scale) and ellipsis flags of the
// pre-029 wrapper (a frozen copy in tests/ui/reference/), over 100 000 seeded strings × widths 1..60 cells × max lines
// 0..4.

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "reference/ui_wrap_cells.hpp"
#include "test.hpp"
#include "text/text_wrap.hpp"
#include "text/utf8.hpp"

namespace {

/// A seeded Latin-1 string (UTF-8): words, spaces runs, hyphens, '\n', '\r', two-byte letters, now and then a bad byte.
int make_string(std::uint32_t& seed, char* out, int capacity) {
    auto next = [&] { return (seed = seed * 1664525u + 1013904223u) >> 8; };
    const int length = static_cast<int>(next() % 60);
    int n = 0;
    for (int i = 0; i < length && n + 2 < capacity; ++i) {
        const std::uint32_t r = next() % 100;
        if (r < 55) out[n++] = static_cast<char>('a' + next() % 26);
        else if (r < 70) out[n++] = ' ';
        else if (r < 75) out[n++] = '-';
        else if (r < 79) out[n++] = '\n';
        else if (r < 81) out[n++] = '\r';
        else if (r < 95) { // é, ç, ü, ß, NBSP …: two bytes of U+00A0..U+00FF
            const std::uint32_t cp = 0xA0 + next() % 0x60;
            out[n++] = static_cast<char>(0xC0 | (cp >> 6));
            out[n++] = static_cast<char>(0x80 | (cp & 0x3F));
        } else if (r < 98) {
            out[n++] = static_cast<char>('A' + next() % 26);
        } else {
            out[n++] = static_cast<char>(0xFF); // invalid UTF-8: one replacement cell
        }
    }
    out[n] = 0;
    return n;
}

} // namespace

NNH_TEST(ui_seam_wrapper_in_pixels_equals_the_cell_wrapper_for_latin1) {
    std::uint32_t seed = 28029;
    char text[160];
    int mismatches = 0, lines = 0;
    for (int round = 0; round < 100000; ++round) {
        const int length = make_string(seed, text, sizeof text);
        const int width = 1 + static_cast<int>((seed >> 3) % 60);
        const int max_lines = static_cast<int>((seed >> 11) % 5);
        const int scale = 1 + static_cast<int>((seed >> 17) % 3);
        nnh::detail::TextFace face;
        face.scale = scale;
        ui_reference::Wrapper old(text, length, width, max_lines);
        nnh::detail::TextWrapper now(face, text, length, width * 8 * scale, max_lines);
        ui_reference::WrapLine a;
        nnh::detail::WrapLine b;
        for (;;) {
            const bool more_a = old.next(a);
            const bool more_b = now.next(b);
            if (more_a != more_b) {
                ++mismatches;
                break;
            }
            if (!more_a) break;
            ++lines;
            const bool same = a.start == b.start && a.bytes == b.bytes && a.cells == b.chars &&
                              a.cells * 8 * scale == b.width * scale && a.ellipsis == b.ellipsis;
            if (!same) {
                if (++mismatches <= 3) std::printf("    '%s' width %d lines %d\n", text, width, max_lines);
                break;
            }
        }
    }
    CHECK(mismatches == 0);
    CHECK(lines > 100000);
}

NNH_TEST(ui_seam_longest_word_in_pixels_equals_the_cell_count_for_latin1) {
    std::uint32_t seed = 2029;
    char text[160];
    int mismatches = 0;
    for (int round = 0; round < 20000; ++round) {
        const int length = make_string(seed, text, sizeof text);
        const nnh::detail::TextFace face;
        if (nnh::detail::longest_word_width(face, text, length) != ui_reference::longest_word_cells(text, length) * 8)
            ++mismatches;
    }
    CHECK(mismatches == 0);
}

// Part 2: widgets with the built-in font — label, wrapping label and button rectangles from the pixel seam equal the
// cell arithmetic of spec 028 (the frozen wrapper's lines × 10, widest cells × 8), over seeded Latin-1 texts.
#include "nnh/app.hpp"
#include "nnh/platform_fake.hpp"
#include "nnh/ui.hpp"

namespace {

int widest_cells(const char* text, int length) {
    int widest = 0, cells = 0;
    for (const char* p = text; p < text + length && *p != 0;) {
        const char32_t c = nnh::detail::next_codepoint(p);
        if (c == U'\n') cells = 0;
        else if (c != U'\r' && ++cells > widest) widest = cells;
    }
    return widest;
}

} // namespace

NNH_TEST(ui_seam_widget_rectangles_equal_the_cell_arithmetic) {
    nnh::shutdown();
    nnh::platform_fake::reset();
    CHECK(nnh::init());
    std::uint32_t seed = 9028;
    char text[160];
    int mismatches = 0;
    for (int round = 0; round < 3000; ++round) {
        const int length = make_string(seed, text, sizeof text);
        if (length == 0) continue;
        const int width_cells = 1 + static_cast<int>((seed >> 5) % 30);
        auto made = nnh::Ui::create({.widgets = 8, .styles = 1});
        if (!made) continue;
        nnh::Ui ui = static_cast<nnh::Ui&&>(made.value());
        nnh::UiStyle st;
        st.padding[0] = st.padding[1] = st.padding[2] = st.padding[3] = 0;
        const nnh::UiStyleId style = ui.add_style(st);
        // each widget alone under a root (a root places its children at their own size)
        const nnh::UiWidget plain = ui.label(ui.root(nnh::Screen::Top), {.text = text, .style = style});
        const nnh::UiWidget wrapped = ui.label(ui.root(nnh::Screen::Bottom), {.text = text, .style = style,
                                                                                .layout = {.width = width_cells * 8},
                                                                                .wrap = true});
        auto made2 = nnh::Ui::create({.widgets = 4, .styles = 1});
        if (!made2) continue;
        nnh::Ui ui2 = static_cast<nnh::Ui&&>(made2.value());
        const nnh::UiWidget button = ui2.button(ui2.root(nnh::Screen::Top), {.text = text, .style = ui2.add_style(st)});
        ui_reference::Wrapper w(text, length, width_cells, 0);
        ui_reference::WrapLine line;
        int lines = 0;
        while (w.next(line)) ++lines;
        const int cells = widest_cells(text, length);
        const bool ok = ui.rect(plain).w == cells * 8 && ui.rect(wrapped).h == lines * 10 && ui2.rect(button).w == cells * 8;
        if (!ok && ++mismatches <= 3) {
            std::printf("    %d cells %d lines: plain %d wrapped h %d button %d\n", cells, lines, ui.rect(plain).w,
                        ui.rect(wrapped).h, ui2.rect(button).w);
        }
    }
    CHECK(mismatches == 0);
    nnh::shutdown();
}
