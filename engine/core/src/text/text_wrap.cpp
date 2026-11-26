// The shared wrapper (text_wrap.hpp). Spec 028's wrapper (ui_wrap.cpp) generalised from cells to a face's pixels; the
// line is measured as the run walk measures it (text_runs.hpp), so a wrapped line's width is the width it draws.

#include "text/text_wrap.hpp"

#include "text/text_runs.hpp"
#include "text/utf8.hpp"

namespace nnh::detail {
namespace {

/// The code point at byte `pos` of `text` (stops at `length`); `next` is the byte after it.
char32_t peek(const char* text, int length, int pos, int& next) {
    if (pos >= length) {
        next = pos;
        return 0;
    }
    const char* p = text + pos;
    const char32_t c = next_codepoint_fast(p);
    next = static_cast<int>(p - text);
    if (next > length) next = length;
    return c;
}

int kind_of(std::uint8_t breaks) { return breaks & kBreakKindMask; }

/// Skips the break spaces from `pos`.
int skip_spaces(const char* text, int length, int pos) {
    int next = pos;
    while (pos < length && kind_of(break_class_fast(peek(text, length, pos, next))) == kBreakSpace) pos = next;
    return pos;
}

/// A line being measured: the pen, the width so far (pen or ink, at scale 1), the glyph kerning pairs with.
struct Pen {
    int pen = 0;
    int width = 0;
    FaceGlyph prev;
};

struct Step {
    FaceGlyph g;
    int pen = 0;
    int width = 0;
    bool transparent = true; ///< '\r' and zero-width characters keep the kerning pair
};

Step measure(const TextFace& face, const Pen& at, char32_t c) {
    Step s;
    s.pen = at.pen;
    s.width = at.width;
    if (c == U'\r') return s;
    s.g = face_glyph(face, c);
    s.transparent = !face.builtin() && (s.g.breaks & kBreakZeroWidth) != 0;
    RunItem it;
    it.g = s.g;
    it.pen = at.pen + (s.transparent ? 0 : face_kerning(face, at.prev, s.g));
    it.advance = s.g.advance;
    it.draw = s.g.draws;
    s.pen = it.pen + it.advance;
    const int ink = ink_right(face, it);
    s.width = at.width > ink ? at.width : ink;
    s.width = s.width > s.pen ? s.width : s.pen;
    return s;
}

void commit(Pen& at, const Step& s) {
    at.pen = s.pen;
    at.width = s.width;
    if (!s.transparent) at.prev = s.g;
}

/// A code point of the line, for kinsoku's look-back.
struct Seen {
    int pos = 0;    ///< its first byte
    int width = 0;  ///< the line's width before it
    int chars = 0;  ///< characters before it
    std::uint8_t breaks = 0;
};

/// The place to break before an ID character `c` at `pos`: the boundary itself, or one or two characters back when
/// kinsoku forbids it; the boundary itself when no place within two characters is allowed (seen[0] is the latest).
Seen id_break(const Seen* seen, int count, int start, int pos, int width, int chars, std::uint8_t c) {
    const auto allowed = [](std::uint8_t left, std::uint8_t right) {
        return kind_of(left) == kBreakIdeographic && kind_of(right) == kBreakIdeographic &&
               (left & kBreakNoEnd) == 0 && (right & kBreakNoStart) == 0;
    };
    const Seen here{pos, width, chars, c};
    if (allowed(seen[0].breaks, c)) return here;
    for (int back = 1; back <= 2 && back < count; ++back) {
        if (seen[back - 1].pos > start && allowed(seen[back].breaks, seen[back - 1].breaks)) return seen[back - 1];
    }
    return here;
}

} // namespace

TextWrapper::TextWrapper(const TextFace& face, const char* text, int length, int max_width, int max_lines)
    : face_(face), text_(text), length_(text != nullptr && length > 0 ? length : 0), max_width_(max_width),
      max_lines_(max_lines) {
    if (length_ == 0) done_ = true;
}

bool TextWrapper::next(WrapLine& line) {
    if (done_) return false;
    const int scale = face_.scale;
    const int start = pos_;
    int pos = start, chars = 0;
    Pen pen;
    int break_end = -1, break_width = 0, break_chars = 0, break_next = -1; // the last break opportunity
    int end = -1, end_width = 0, end_chars = 0, next_start = length_;
    bool newline = false, prev_space = false;
    Seen ring[3]; // the last three code points, newest at ring[head] (a ring: no copies per character; T10.3)
    int head = 0, seen_count = 0;
    while (pos < length_) {
        int after = pos;
        const char32_t c = peek(text_, length_, pos, after);
        if (c == U'\n') {
            end = pos;
            end_width = pen.width;
            end_chars = chars;
            next_start = after;
            newline = true;
            break;
        }
        const Step s = measure(face_, pen, c);
        const std::uint8_t breaks = c == U'\r' ? 0 : s.g.breaks;
        const int kind = kind_of(breaks);
        if (kind == kBreakIdeographic && seen_count > 0 && kind_of(ring[head].breaks) == kBreakIdeographic) {
            const Seen seen[3] = {ring[head], ring[(head + 2) % 3], ring[(head + 1) % 3]}; // newest first
            const Seen at = id_break(seen, seen_count, start, pos, pen.width, chars, breaks);
            break_end = at.pos;
            break_width = at.width;
            break_chars = at.chars;
            break_next = at.pos;
        }
        if (max_width_ > 0 && s.width * scale > max_width_) {
            if (kind == kBreakSpace) { // the line ends where the spaces start; they are dropped
                end = prev_space ? break_end : pos;
                end_width = prev_space ? break_width : pen.width;
                end_chars = prev_space ? break_chars : chars;
                next_start = skip_spaces(text_, length_, pos);
            } else if (break_end >= 0 && break_chars > 0) {
                end = break_end;
                end_width = break_width;
                end_chars = break_chars;
                next_start = break_next;
            } else { // a word longer than the line: by characters (at least one per line)
                if (chars == 0) {
                    pos = after;
                    commit(pen, s);
                    chars = c == U'\r' ? 0 : 1;
                }
                end = pos;
                end_width = pen.width;
                end_chars = chars;
                next_start = pos;
            }
            break;
        }
        if (kind == kBreakSpace) {
            if (!prev_space) { // the first space of a run
                break_end = pos;
                break_width = pen.width;
                break_chars = chars;
                break_next = skip_spaces(text_, length_, pos);
            }
            prev_space = true;
        } else {
            prev_space = false;
            if (kind == kBreakHyphen) {
                break_end = after;
                break_width = s.width;
                break_chars = chars + 1;
                break_next = after;
            }
        }
        if (c != U'\r') {
            head = head == 2 ? 0 : head + 1;
            ring[head] = Seen{pos, pen.width, chars, breaks};
            seen_count = seen_count < 3 ? seen_count + 1 : 3;
            ++chars;
        }
        commit(pen, s);
        pos = after;
    }
    if (end < 0) { // the end of the text
        end = length_;
        end_width = pen.width;
        end_chars = chars;
        next_start = length_;
    }
    ++lines_;
    line.start = start;
    line.bytes = end - start;
    line.width = end_width;
    line.chars = end_chars;
    line.ellipsis = false;
    const bool more = newline || skip_spaces(text_, length_, next_start) < length_;
    if (max_lines_ > 0 && lines_ >= max_lines_ && more) {
        cut_for_ellipsis(line, end);
        done_ = true;
        return true;
    }
    pos_ = next_start;
    if (!newline && pos_ >= length_) done_ = true;
    if (newline && pos_ >= length_) pos_ = length_; // a final '\n' leaves an empty last line
    return true;
}

void TextWrapper::cut_for_ellipsis(WrapLine& line, int end) const {
    const int scale = face_.scale;
    const int ellipsis = ellipsis_width(face_) * scale;
    const int keep = max_width_ > 0 ? (max_width_ > ellipsis ? max_width_ - ellipsis : 0) : line.width * scale;
    Pen pen;
    int p = line.start, chars = 0, last_end = line.start, last_width = 0, last_chars = 0;
    while (p < end && pen.width * scale < keep) {
        int after = p;
        const char32_t c = peek(text_, length_, p, after);
        const Step s = measure(face_, pen, c);
        if (s.width * scale > keep) break;
        commit(pen, s);
        chars += c == U'\r' ? 0 : 1;
        p = after;
        if (kind_of(break_class_fast(c)) != kBreakSpace) {
            last_end = p;
            last_width = pen.width;
            last_chars = chars;
        }
    }
    line.bytes = last_end - line.start;
    line.width = last_width;
    line.chars = last_chars;
    line.ellipsis = true;
}

int longest_word_width(const TextFace& face, const char* text, int length) {
    int longest = 0, pos = 0;
    Pen pen;
    std::uint8_t prev = 0;
    while (text != nullptr && pos < length) {
        int after = pos;
        const char32_t c = peek(text, length, pos, after);
        pos = after;
        const std::uint8_t breaks = c == U'\r' ? 0 : break_class_fast(c);
        if (kind_of(breaks) == kBreakSpace || c == U'\n') {
            pen = Pen{};
            prev = 0;
            continue;
        }
        if (kind_of(breaks) == kBreakIdeographic && kind_of(prev) == kBreakIdeographic) pen = Pen{}; // a piece each
        commit(pen, measure(face, pen, c));
        if (pen.width > longest) longest = pen.width;
        if (kind_of(breaks) == kBreakHyphen) pen = Pen{};
        if (c != U'\r') prev = breaks;
    }
    return longest * face.scale;
}

const char* ellipsis_text(const TextFace& face) {
    return !face.builtin() && face.fonts[0]->header.ellipsis != font::kNoGlyph ? "\xE2\x80\xA6" : "...";
}

int ellipsis_width(const TextFace& face) {
    const char* e = ellipsis_text(face);
    const char* end = e;
    while (*end != 0) ++end;
    return run_width(face, e, end);
}

} // namespace nnh::detail
