// The debug pass (docs/architecture/rendering.md): debug text and overlay rectangles recorded during the
// frame become glyph sprites of the built-in font and filled rectangles, submitted per screen after the
// 2D passes. It has its own fixed capacity (kDebugPassCapacity), so debug output never uses the game's
// AppConfig::gfx.max_draws.

#include "debug/debug_list.hpp"
#include "debug/dev_tools.hpp"
#include "gfx/gfx3d_hook.hpp"
#include "gfx/gfx_state.hpp"
#include "nnh/platform/video.hpp"
#include "text/font_atlas.hpp"
#include "text/font_texture.hpp"
#include "text/layout.hpp"

namespace nnh::detail {
namespace {

constexpr std::uint8_t kDebugLayer = 15;

platform::DrawItem2D rect_item(const DebugText& e) {
    platform::DrawItem2D item;
    item.kind = platform::DrawItem2D::Kind::FillRect;
    item.layer = kDebugLayer;
    item.x = e.x;
    item.y = e.y;
    item.w = e.w;
    item.h = e.h;
    item.color = e.color;
    return item;
}

} // namespace

void debug_pass_submit(const DebugList& list) {
    GfxState& g = gfx_state();
    if (!g.ready || g.debug_items == nullptr) return;
    const TextureData* font = font_texture_ready()
                                  ? g.textures.get(font_texture_slot(), font_texture_generation())
                                  : nullptr;
    for (const Screen screen : {Screen::Top, Screen::Bottom}) {
        int count = 0;
        for (int i = 0; i < list.count(); ++i) {
            const DebugText& e = list.at(i);
            if (e.screen != screen) continue;
            if (e.kind == DebugText::Kind::Rect) {
                if (count < kDebugPassCapacity) g.debug_items[count++] = rect_item(e);
                continue;
            }
            if (font == nullptr) continue;
            layout_text(list.text(i), Vec2{static_cast<float>(e.x), static_cast<float>(e.y)}, 1, TextAlign::Left,
                        [&](const GlyphPlacement& placed) {
                            if (count >= kDebugPassCapacity) return; // the rest is dropped
                            platform::DrawItem2D& item = g.debug_items[count++];
                            item = platform::DrawItem2D{};
                            item.kind = platform::DrawItem2D::Kind::Sprite;
                            item.layer = kDebugLayer;
                            item.texture = font->platform;
                            item.region = glyph_region(placed.glyph);
                            item.x = placed.x;
                            item.y = placed.y;
                            item.w = kFontCellSize;
                            item.h = kFontCellSize;
                            item.color = e.color;
                        });
        }
        if (count == 0) continue;
        platform::draw_2d(screen, g.debug_items, count);
        // A stereo frame (spec 026 FR-15): the top screen's debug items in the right eye too, at screen depth.
        if (screen == Screen::Top && g_gfx_3d_ops != nullptr && g_gfx_3d_ops->eyes_this_frame() == 2) {
            g_gfx_3d_ops->select_eye(1);
            platform::draw_2d(screen, g.debug_items, count);
            g_gfx_3d_ops->select_eye(0);
        }
    }
}

} // namespace nnh::detail
