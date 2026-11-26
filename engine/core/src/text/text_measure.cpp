// Measuring text as draw_text lays it out (nnh/gfx.hpp measure_text(utf8, options), measure_chars; spec 029 FR-8): the
// same lines, widths and pens as text_draw.cpp, through the same face. No allocation.

#include "nnh/gfx.hpp"
#include "text/font_resolve.hpp"
#include "text/layout.hpp"
#include "text/text_draw.hpp"
#include "text/text_runs.hpp"

namespace nnh {
namespace {

/// A caret coordinate: past ±32767 pixels (a huge text at a big scale) it stays at the edge (review N5).
std::int16_t clamp16(int v) { return static_cast<std::int16_t>(v < -32768 ? -32768 : v > 32767 ? 32767 : v); }

} // namespace

Vec2 gfx::measure_text(const char* utf8, const TextOptions& options) {
    if (utf8 == nullptr || *utf8 == 0) return {};
    if (!detail::uses_text_layout(options)) return detail::measure(utf8, options.scale);
    const detail::TextFace face = detail::face_for(options.font, options.scale);
    int widest = 0, lines = 0;
    detail::for_each_text_line(face, utf8, options, [&](const detail::WrapLine& line, int) {
        const int w = detail::text_line_width(face, line);
        widest = w > widest ? w : widest;
        ++lines;
    });
    const int s = face.scale;
    return {static_cast<float>(widest * s), static_cast<float>(lines * detail::face_line_height(face) * s)};
}

int gfx::measure_chars(const char* utf8, const TextOptions& options, TextCaret* out, int capacity) {
    if (utf8 == nullptr || out == nullptr || capacity <= 0) return 0;
    const detail::TextFace face = detail::face_for(options.font, options.scale);
    const int s = face.scale;
    const int line_height = detail::face_line_height(face);
    int written = 0;
    detail::for_each_text_line(face, utf8, options, [&](const detail::WrapLine& line, int index) {
        const int x0 = detail::align_offset(options.align, detail::text_line_width(face, line));
        detail::walk_run(face, utf8 + line.start, utf8 + line.start + line.bytes, [&](const detail::RunItem& it) {
            if (written >= capacity) return;
            TextCaret& c = out[written++];
            c.byte = line.start + it.byte;
            c.x = clamp16((x0 + it.pen) * s);
            c.y = clamp16(index * line_height * s);
            c.advance = clamp16(it.advance * s);
            c.line = clamp16(index);
        });
    });
    return written;
}

} // namespace nnh
