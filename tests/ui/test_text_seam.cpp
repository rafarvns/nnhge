// The text seam's measure (spec 028 T10.3): widest line, lines and total cells in one walk — the same numbers the
// wrapper gives a non-wrapping label, so layout's third pass can reuse them instead of wrapping again.

#include <cstring>

#include "test.hpp"
#include "ui/ui_block.hpp"
#include "ui/ui_text_seam.hpp"
#include "text/text_wrap.hpp"

using nnh::detail::TextSize;
using nnh::detail::TextFace;
using nnh::detail::TextWrapper;
using nnh::detail::WrapLine;

namespace {

/// Lines and total cells as the wrapper (no width, no line limit) counts them.
TextSize by_wrapper(const char* text) {
    const TextFace builtin;
    TextWrapper w(builtin, text, static_cast<int>(std::strlen(text)), 0, 0);
    WrapLine line;
    TextSize t;
    while (w.next(line)) {
        ++t.lines;
        t.total += line.chars;
        if (line.width > t.width) t.width = line.width; // the built-in face: chars × 8
    }
    return t;
}

bool same(const TextSize& a, const TextSize& b) { return a.width == b.width && a.lines == b.lines && a.total == b.total; }

/// A block whose looks use the built-in font (the seam reads only its font slots).
const nnh::detail::UiBlock g_block{};
const nnh::UiTextLook g_look{};

TextSize seam_measure(const char* text) { return nnh::detail::seam_measure(g_block, g_look, text); }

} // namespace

NNH_TEST(ui_seam_measure_counts_widest_lines_and_total) {
    const TextSize t = seam_measure("ab\ncdef\r\nx");
    CHECK(t.width == 32 && t.lines == 3 && t.total == 7);
    CHECK(same(seam_measure(""), TextSize{}));
}

NNH_TEST(ui_seam_measure_agrees_with_the_wrapper) {
    const char* texts[] = {"Hello", "two\nlines", "a\n", "\n", "\n\nx", "caf\xC3\xA9 ol\xC3\xA9", "a\xFF" "b\xE2\x82",
                           "tab\there", "  spaced  ", "x\r\ny"};
    bool ok = true;
    for (const char* t : texts) ok = ok && same(seam_measure(t), by_wrapper(t));
    CHECK(ok);
}
