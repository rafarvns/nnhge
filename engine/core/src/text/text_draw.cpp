// draw_text's run layout (text_draw.hpp): positions are the game's float position plus integer pens × scale.

#include "text/text_draw.hpp"

#include "gfx/gfx_state.hpp"
#include "log/engine_channels.hpp"
#include "text/font_atlas.hpp"
#include "text/font_resolve.hpp"
#include "text/font_texture.hpp"
#include "text/text_missing.hpp"
#include "text/text_runs.hpp"

namespace nnh::detail {
namespace {

using Kind = platform::DrawItem2D::Kind;

struct Emit {
    Screen screen;
    Vec2 origin;
    const TextOptions& o;
    const TextFace& face;
    float s; // scale
};

void sprite(const Emit& e, int slot, std::uint32_t generation, Region region, float x, float y, Color color) {
    DrawCommand c;
    c.kind = Kind::Sprite;
    c.screen = e.screen;
    c.layer = gfx_clamp_layer(e.o.layer);
    c.ignore_camera = e.o.ignore_camera;
    c.texture_slot = slot;
    c.texture_generation = generation;
    c.region = region;
    c.position = {x, y};
    c.scale = {e.s, e.s};
    c.color = color;
    gfx_record(c);
}

void box(const Emit& e, float x, float y, float w, float h, Color color) {
    DrawCommand c;
    c.kind = Kind::RectOutline;
    c.screen = e.screen;
    c.layer = gfx_clamp_layer(e.o.layer);
    c.ignore_camera = e.o.ignore_camera;
    c.position = {x, y};
    c.second = {w, h};
    c.color = color;
    gfx_record(c);
}

/// One drawn item at (px, py), pixels at scale 1 from the text's origin: shadow, outline, glyph.
void emit(const Emit& e, const RunItem& it, int px, int py) {
    const float s = e.s;
    const auto at = [&](int x, int y) { return Vec2{e.origin.x + static_cast<float>(x) * s, e.origin.y + static_cast<float>(y) * s}; };
    if (it.g.glyph < 0) { // the hollow box at the font's size
        const float w = static_cast<float>(face_box_advance(e.face) - 2) * s;
        const float h = static_cast<float>(face_ascent(e.face) - 1) * s;
        const Vec2 p = at(px + 1, py + 1);
        if (e.o.shadow) box(e, p.x + s, p.y + s, w, h, e.o.shadow_color);
        box(e, p.x, p.y, w, h, e.o.color);
        return;
    }
    if (e.face.builtin()) {
        const Region r = glyph_region(it.g.glyph);
        const Vec2 p = at(px, py);
        if (e.o.shadow) sprite(e, font_texture_slot(), font_texture_generation(), r, p.x + s, p.y + s, e.o.shadow_color);
        sprite(e, font_texture_slot(), font_texture_generation(), r, p.x, p.y, e.o.color);
        return;
    }
    const FontBlock& f = *e.face.fonts[it.g.font];
    const font::GlyphRecord& g = f.glyphs[it.g.glyph];
    const Region r{g.x, g.y, g.w, g.h};
    const Vec2 p = at(px + g.ox, py + g.oy);
    if (e.o.shadow) sprite(e, f.page_slot[g.page], f.page_generation[g.page], r, p.x + s, p.y + s, e.o.shadow_color);
    if (e.o.outline_color.a > 0 && f.outlines != nullptr && (g.flags & font::kGlyphOutline) != 0) {
        const font::OutlineRecord& o = f.outlines[it.g.glyph];
        const int page = o.xp >> 12;
        const Vec2 q = at(px + o.ox, py + o.oy);
        sprite(e, f.page_slot[page], f.page_generation[page], {o.xp & 0xFFF, o.y, o.w, o.h}, q.x, q.y, e.o.outline_color);
    }
    sprite(e, f.page_slot[g.page], f.page_generation[g.page], r, p.x, p.y, e.o.color);
}

/// The outline colour on a font without outlines: ignored, said once per font slot (Debug/Development).
void warn_no_outline([[maybe_unused]] const TextFace& face) {
#if !defined(NNH_FLAVOR_RELEASE)
    static std::uint32_t warned = 0;
    const std::uint32_t bit = 1u << (face.fonts[0]->self_slot & 31u);
    if ((warned & bit) == 0) NNH_LOG_WARN(gfx, "outline_color ignored: the font has no outline glyphs");
    warned |= bit;
#endif
}

} // namespace

void draw_text_layout(Screen screen, Vec2 position, const char* utf8, const TextOptions& options) {
    const TextFace face = face_for(options.font, options.scale);
    if (face.builtin() && !font_texture_ready()) return;
    if (options.outline_color.a > 0 && !face.builtin() && face.fonts[0]->outlines == nullptr) warn_no_outline(face);
    const Emit e{screen, position, options, face, static_cast<float>(face.scale)};
    const int line_height = face_line_height(face);
    if (options.wrap_width <= 0 && options.max_lines <= 0) { // no wrapping: lines end at '\n'; one walk each (two aligned)
        int index = 0;
        for (const char* line = utf8;; ++index) {
            const char* end = line;
            while (*end != 0 && *end != '\n') ++end;
            const int x0 = options.align == TextAlign::Left ? 0 : align_offset(options.align, run_width(face, line, end));
            walk_run(face, line, end, [&](const RunItem& it) {
                if (it.g.missing) text_missing(face.fonts[0]->self_slot, it.cp, face.fonts[0]->name);
                if (it.draw) emit(e, it, x0 + it.pen, index * line_height);
            });
            if (*end == 0) return;
            line = end + 1;
        }
    }
    for_each_text_line(face, utf8, options, [&](const WrapLine& line, int index) {
        const int x0 = align_offset(options.align, text_line_width(face, line));
        const int y0 = index * line_height;
        walk_run(face, utf8 + line.start, utf8 + line.start + line.bytes, [&](const RunItem& it) {
            if (it.g.missing) text_missing(face.fonts[0]->self_slot, it.cp, face.fonts[0]->name);
            if (it.draw) emit(e, it, x0 + it.pen, y0);
        });
        if (line.ellipsis) {
            const char* dots = ellipsis_text(face);
            const char* end = dots;
            while (*end != 0) ++end;
            walk_run(face, dots, end, [&](const RunItem& it) {
                if (it.draw) emit(e, it, x0 + line.width + it.pen, y0);
            });
        }
    });
}

} // namespace nnh::detail
