// A frozen copy of spec 028's wrapper (engine/core/src/ui/ui_wrap.cpp before spec 029), the reference the shared
// text wrapper is proven against (tests/ui/test_ui_seam_equivalence.cpp). Never change it.

#include "ui_wrap_cells.hpp"

#include "text/utf8.hpp"

namespace ui_reference {

using nnh::detail::next_codepoint;
namespace {

bool is_break_space(char32_t c) { return c == U' '; }

/// The code point at byte `pos` of `text` (stops at `length`); `next` is the byte after it.
char32_t peek(const char* text, int length, int pos, int& next) {
    if (pos >= length) {
        next = pos;
        return 0;
    }
    const char* p = text + pos;
    const char32_t c = next_codepoint(p);
    next = static_cast<int>(p - text);
    if (next > length) next = length;
    return c;
}

int cells_of(char32_t c) { return c == U'\r' ? 0 : 1; }

/// Skips the spaces from `pos`.
int skip_spaces(const char* text, int length, int pos) {
    int next = pos;
    while (pos < length && is_break_space(peek(text, length, pos, next))) pos = next;
    return pos;
}

} // namespace

Wrapper::Wrapper(const char* text, int length, int max_cells, int max_lines)
    : text_(text), length_(text != nullptr && length > 0 ? length : 0), max_cells_(max_cells),
      max_lines_(max_lines) {
    if (length_ == 0) done_ = true;
}

bool Wrapper::next(WrapLine& line) {
    if (done_) return false;
    const int start = pos_;
    int pos = start, cells = 0;
    int break_end = -1, break_cells = 0, break_next = -1; // the last break opportunity on this line
    int end = -1, end_cells = 0, next_start = length_;
    bool newline = false, prev_space = false;
    while (pos < length_) {
        int after = pos;
        const char32_t c = peek(text_, length_, pos, after);
        if (c == U'\n') {
            end = pos;
            end_cells = cells;
            next_start = after;
            newline = true;
            break;
        }
        const int w = cells_of(c);
        if (max_cells_ > 0 && cells + w > max_cells_) {
            if (is_break_space(c)) { // the line ends where the spaces start; they are dropped
                end = prev_space ? break_end : pos;
                end_cells = prev_space ? break_cells : cells;
                next_start = skip_spaces(text_, length_, pos);
            } else if (break_end >= 0 && break_cells > 0) {
                end = break_end;
                end_cells = break_cells;
                next_start = break_next;
            } else { // a word longer than the line: by characters (at least one per line)
                if (cells == 0) {
                    pos = after;
                    cells = w;
                }
                end = pos;
                end_cells = cells;
                next_start = pos;
            }
            break;
        }
        if (is_break_space(c)) {
            if (!prev_space) { // the first space of a run
                break_end = pos;
                break_cells = cells;
                break_next = skip_spaces(text_, length_, pos);
            }
            prev_space = true;
        } else {
            prev_space = false;
            if (c == U'-') {
                break_end = after;
                break_cells = cells + w;
                break_next = after;
            }
        }
        cells += w;
        pos = after;
    }
    if (end < 0) { // the end of the text
        end = length_;
        end_cells = cells;
        next_start = length_;
    }
    ++lines_;
    line.start = start;
    line.bytes = end - start;
    line.cells = end_cells;
    line.ellipsis = false;
    const bool more = newline || skip_spaces(text_, length_, next_start) < length_;
    if (max_lines_ > 0 && lines_ >= max_lines_ && more) {
        // cut to width − 3 cells (when there is a width), drop trailing spaces, then "..."
        const int keep = max_cells_ > 0 ? (max_cells_ > 3 ? max_cells_ - 3 : 0) : line.cells;
        int p = start, kept = 0, last_end = start, last_cells = 0;
        while (p < end && kept < keep) {
            int after = p;
            const char32_t c = peek(text_, length_, p, after);
            kept += cells_of(c);
            p = after;
            if (!is_break_space(c)) {
                last_end = p;
                last_cells = kept;
            }
        }
        line.bytes = last_end - start;
        line.cells = last_cells;
        line.ellipsis = true;
        done_ = true;
        return true;
    }
    pos_ = next_start;
    if (!newline && pos_ >= length_) done_ = true;
    if (newline && pos_ >= length_) { // a final '\n' leaves an empty last line
        pos_ = length_;
    }
    return true;
}

int longest_word_cells(const char* text, int length) {
    int longest = 0, current = 0, pos = 0;
    while (text != nullptr && pos < length) {
        int after = pos;
        const char32_t c = peek(text, length, pos, after);
        if (is_break_space(c) || c == U'\n') {
            current = 0;
        } else {
            current += cells_of(c);
            if (current > longest) longest = current;
            if (c == U'-') current = 0;
        }
        pos = after;
    }
    return longest;
}

} // namespace ui_reference
