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
