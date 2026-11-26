#pragma once
// The shared wrapper (spec 029 FR-10; spec 028 FR-5, FR-23; plan 029 "Wrapping"): one implementation for draw_text's
// wrap_width/max_lines and spec 028's labels, in pixels through a face. Break opportunities: '\n' (mandatory); after a
// run of break spaces (U+0020, U+3000: dropped at the break); after '-' and U+2010; between two ideographic (ID)
// characters, where kinsoku applies — a break before a no-start character or after a no-end one moves back one
// character, at most twice, else stays where it was; a word longer than the line breaks by characters (at least one per
// line). With max_lines, the last line is cut so it plus the ellipsis (the font's "…", else "...") fits, its trailing
// spaces dropped. Every new rule is keyed to code points ≥ U+2000, so Latin-1 text wraps as spec 028's wrapper did
// (tests/ui/test_ui_seam_equivalence.cpp). Pure; never allocates. Tests: tests/text/test_text_wrap.cpp.

#include "text/text_face.hpp"

namespace nnh::detail {

struct WrapLine {
    int start = 0;         ///< byte offset of the line
    int bytes = 0;         ///< bytes drawn
    int width = 0;         ///< pixels at scale 1 (the ellipsis not counted)
    int chars = 0;         ///< code points drawn but '\r' (what a typewriter reveals; the ellipsis not counted)
    bool ellipsis = false; ///< ellipsis_text() follows
};

/// Iterates the lines of `text` (`length` bytes) at `max_width` pixels at the face's scale (≤ 0: no limit) and at most
/// `max_lines` lines (≤ 0: any). Empty text has no line.
class TextWrapper {
public:
    TextWrapper(const TextFace& face, const char* text, int length, int max_width, int max_lines);
    /// The wrapper keeps a reference to `face`: a temporary would dangle.
    TextWrapper(const TextFace&& face, const char* text, int length, int max_width, int max_lines) = delete;
    bool next(WrapLine& line);

private:
    const TextFace& face_;
    const char* text_;
    int length_;
    int max_width_;
    int max_lines_;
    int pos_ = 0;
    int lines_ = 0;
    bool done_ = false;

    void cut_for_ellipsis(WrapLine& line, int end) const;
};

/// The pixels (at the face's scale) of the widest unbreakable piece: spaces and '\n' end one, a hyphen ends one after
/// it, an ID character is a piece of its own.
int longest_word_width(const TextFace& face, const char* text, int length);

/// The ellipsis a face draws: "…" (U+2026) when its font has it, else "..." (the built-in font: "...").
const char* ellipsis_text(const TextFace& face);
/// Its width at scale 1.
int ellipsis_width(const TextFace& face);

} // namespace nnh::detail
