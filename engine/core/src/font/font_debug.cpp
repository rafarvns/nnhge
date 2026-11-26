// Font debugging (nnh/font.hpp Font::dump, font::debug_draw_pages; spec 029 FR-33): Debug/Development only — Release
// compiles them empty, so no text of theirs reaches a Release binary.

#include "font/font_internal.hpp"
#include "font/font_table.hpp"
#include "gfx/gfx_state.hpp"
#include "nnh/build.hpp"
#include "nnh/log.hpp"
#include "nnh/texture.hpp"

namespace nnh {

NNH_LOG_CHANNEL_EXTERN(font);

void Font::dump() const {
#if !defined(NNH_FLAVOR_RELEASE)
    const int slot = detail::font_table_slot(id_);
    const detail::FontBlock* f = slot >= 0 ? detail::font_table_block(id_) : nullptr;
    if (f == nullptr) {
        NNH_LOG_INFO(font, "font: none (the built-in font)");
        return;
    }
    const detail::font::Header& h = f->header;
    const detail::font::PageRecord& p = f->pages[0];
    NNH_LOG_INFO(font, "font %s: %u glyphs, %u page%s %ux%u %s, size %u line %u ascent %d descent %d, %u kerning pair%s",
                 detail::font_table_extra(slot).path, static_cast<unsigned>(h.glyph_count),
                 static_cast<unsigned>(h.page_count), h.page_count == 1 ? "" : "s", static_cast<unsigned>(p.width),
                 static_cast<unsigned>(p.height), p.format == detail::font::kPageRgba8 ? "RGBA8" : "A4",
                 static_cast<unsigned>(h.size), static_cast<unsigned>(h.line_height), h.ascent, h.descent,
                 static_cast<unsigned>(h.kerning_count), h.kerning_count == 1 ? "" : "s");
    NNH_LOG_INFO(font, "  %zu bytes (block and pages), %u fallback%s, outline %s, licence %s",
                 detail::font_bytes(slot), static_cast<unsigned>(h.fallback_count), h.fallback_count == 1 ? "" : "s",
                 (h.flags & detail::font::kFlagOutline) != 0 ? "yes" : "no", f->licence);
#endif
}

void font::debug_draw_pages(Screen screen, FontRef font, Vec2 at, int page) {
#if !defined(NNH_FLAVOR_RELEASE)
    const detail::FontBlock* f = detail::font_table_block(font.id);
    if (f == nullptr || !detail::gfx_drawing()) return;
    constexpr float kGap = 4;
    // one scale for every page drawn: their row fits the room right of and below `at` (never enlarged)
    float widths = 0, tallest = 0;
    int shown = 0;
    for (int i = 0; i < f->header.page_count; ++i) {
        if (page >= 0 && i != page) continue;
        widths += f->pages[i].width;
        tallest = f->pages[i].height > tallest ? f->pages[i].height : tallest;
        ++shown;
    }
    const float room_w = static_cast<float>(screen_width(screen)) - at.x - kGap * static_cast<float>(shown - 1);
    const float room_h = static_cast<float>(screen_height(screen)) - at.y;
    if (shown == 0 || room_w <= 0 || room_h <= 0) return;
    float k = 1;
    if (widths * k > room_w) k = room_w / widths;
    if (tallest * k > room_h) k = room_h / tallest;
    const ShapeOptions shape{.layer = 15, .ignore_camera = true};
    float x = at.x;
    for (int i = 0; i < f->header.page_count; ++i) {
        if (page >= 0 && i != page) continue;
        const detail::font::PageRecord& p = f->pages[i];
        if (detail::gfx_state().textures.get(f->page_slot[i], f->page_generation[i]) != nullptr) {
            detail::DrawCommand c;
            c.kind = platform::DrawItem2D::Kind::Sprite;
            c.screen = screen;
            c.layer = 15;
            c.ignore_camera = true;
            c.texture_slot = f->page_slot[i];
            c.texture_generation = f->page_generation[i];
            c.region = {0, 0, p.width, p.height};
            c.position = {x, at.y};
            c.scale = {k, k};
            c.color = colors::white;
            detail::gfx_record(c);
            gfx::draw_rect(screen, {x, at.y, p.width * k, p.height * k}, colors::yellow, shape);
            for (std::uint32_t g = 0; g < f->header.glyph_count; ++g) { // each glyph with pixels on this page
                const detail::font::GlyphRecord& r = f->glyphs[g];
                if (r.page != i || r.w == 0 || r.h == 0) continue;
                gfx::draw_rect(screen, {x + r.x * k, at.y + r.y * k, r.w * k, r.h * k}, rgba(0, 220, 255, 255), shape);
            }
        }
        x += static_cast<float>(p.width) * k + kGap;
    }
#else
    (void)screen, (void)font, (void)at, (void)page;
#endif
}

} // namespace nnh
