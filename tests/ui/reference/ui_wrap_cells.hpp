#pragma once
// FROZEN (spec 029): spec 028's wrapper in the built-in font's cells (spec 028 FR-5, FR-23). Code points through detail::next_codepoint (one
// cell each, '\r' none); breaks at spaces (the spaces at a break are dropped) and after '-'; '\n' ends a line; a word
// longer than the line breaks by characters; with max_lines, the last line is cut to width − 3 cells (its trailing
// spaces dropped) and "..." follows when text remains. Pure; unit tested in tests/ui/test_wrap.cpp.

namespace ui_reference {

struct WrapLine {
    int start = 0;          ///< byte offset of the line
    int bytes = 0;          ///< bytes drawn
    int cells = 0;          ///< characters drawn (the ellipsis not counted)
    bool ellipsis = false;  ///< "..." follows
};

/// Iterates the lines of `text` (`length` bytes) at `max_cells` per line (<= 0: no limit) and at most `max_lines`
/// lines (<= 0: any). Empty text has no line.
class Wrapper {
public:
    Wrapper(const char* text, int length, int max_cells, int max_lines);
    bool next(WrapLine& line);

private:
    const char* text_;
    int length_;
    int max_cells_;
    int max_lines_;
    int pos_ = 0;
    int lines_ = 0;
    bool done_ = false;
};

/// Cells of the longest unbreakable piece ("well-known": 5).
int longest_word_cells(const char* text, int length);

} // namespace ui_reference
