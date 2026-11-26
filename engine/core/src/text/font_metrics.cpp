// gfx::font_metrics (nnh/gfx.hpp; spec 029 FR-6): a font's integer metrics from its block, the built-in font's for
// FontRef{}. Its own object: measure_text doesn't link the block's size rule.

#include "font/font_block.hpp"
#include "nnh/gfx.hpp"
#include "text/font_resolve.hpp"

namespace nnh {

FontMetrics gfx::font_metrics(FontRef font) {
    FontMetrics m;
    const detail::TextFace face = detail::face_for(font, 1);
    if (face.builtin()) return m;
    const detail::FontBlock& f = *face.fonts[0];
    m.size = f.header.size;
    m.line_height = f.header.line_height;
    m.ascent = f.header.ascent;
    m.descent = f.header.descent;
    m.glyphs = static_cast<int>(f.header.glyph_count);
    m.pages = f.header.page_count;
    m.bytes = detail::font_block_bytes(f.header);
    for (int i = 0; i < f.header.page_count; ++i) {
        const auto px = std::size_t{f.pages[i].width} * f.pages[i].height;
        m.bytes += f.pages[i].format == detail::font::kPageRgba8 ? px * 4 : px;
    }
    m.outline = (f.header.flags & detail::font::kFlagOutline) != 0;
    m.colour = (f.header.flags & detail::font::kFlagColour) != 0;
    return m;
}

} // namespace nnh
