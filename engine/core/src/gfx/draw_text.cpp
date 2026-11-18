// Text with the built-in font (nnh/gfx.hpp): every visible character becomes a sprite of the font
// texture in the frame's draw list, so text layers, cameras and batching work like any sprite.

#include "gfx/gfx_state.hpp"
#include "nnh/gfx.hpp"
#include "text/font_atlas.hpp"
#include "text/font_texture.hpp"
#include "text/layout.hpp"

namespace nnh {

void gfx::draw_text(Screen screen, Vec2 position, const char* utf8, const TextOptions& options) {
    if (utf8 == nullptr || !detail::gfx_drawing() || !detail::font_texture_ready()) return;
    const int scale = detail::clamp_text_scale(options.scale);
    const auto size = static_cast<float>(scale);

    detail::DrawCommand glyph;
    glyph.kind = platform::DrawItem2D::Kind::Sprite;
    glyph.screen = screen;
    glyph.layer = detail::gfx_clamp_layer(options.layer);
    glyph.ignore_camera = options.ignore_camera;
    glyph.texture_slot = detail::font_texture_slot();
    glyph.texture_generation = detail::font_texture_generation();
    glyph.scale = {size, size};

    detail::layout_text(utf8, position, scale, options.align, [&](const detail::GlyphPlacement& placed) {
        glyph.region = detail::glyph_region(placed.glyph);
        if (options.shadow) {
            glyph.position = {placed.x + size, placed.y + size};
            glyph.color = options.shadow_color;
            detail::gfx_record(glyph);
        }
        glyph.position = {placed.x, placed.y};
        glyph.color = options.color;
        detail::gfx_record(glyph);
    });
}

Vec2 gfx::measure_text(const char* utf8, int scale) { return detail::measure(utf8, scale); }

} // namespace nnh
