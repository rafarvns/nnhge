// The UI's gfx calls (ui_draw_out.hpp).

#include <bit>
#include <cstring>

#include "nnh/gfx.hpp"
#include "nnh/texture.hpp"
#include "ui/ui_draw_out.hpp"
#include "ui/ui_nine_slice.hpp"
#include "ui/ui_text_seam.hpp"

namespace nnh::detail {
namespace {

void fold(const UiBlock& b, std::uint32_t word) {
    std::uint32_t h = b.draw_hash;
    for (int i = 0; i < 4; ++i) {
        h ^= (word >> (8 * i)) & 0xFFu;
        h *= 16777619u;
    }
    b.draw_hash = h;
}
void fold(const UiBlock& b, int v) { fold(b, static_cast<std::uint32_t>(v)); }
void fold(const UiBlock& b, Color c) {
    fold(b, static_cast<std::uint32_t>(c.r | (c.g << 8) | (c.b << 16) | (static_cast<std::uint32_t>(c.a) << 24)));
}
void fold(const UiBlock& b, float f) { fold(b, std::bit_cast<std::uint32_t>(f)); }

void fold_head(const UiBlock& b, int kind, const DrawCtx& d) {
    fold(b, kind);
    fold(b, d.screen == Screen::Bottom ? 1 : 0);
    fold(b, d.layer);
}

Rect frect(const UiRect& r, const DrawCtx& d) {
    return {static_cast<float>(r.x + d.dx), static_cast<float>(r.y + d.dy), static_cast<float>(r.w),
            static_cast<float>(r.h)};
}

} // namespace

Color fade(Color c, int opacity) {
    if (opacity >= 255) return c;
    c.a = static_cast<std::uint8_t>((c.a * opacity + 127) / 255);
    return c;
}

void out_fill(const UiBlock& b, const DrawCtx& d, const UiRect& r, Color color) {
    const Color c = fade(color, d.opacity);
    fold_head(b, 1, d);
    fold(b, r.x + d.dx);
    fold(b, r.y + d.dy);
    fold(b, r.w);
    fold(b, r.h);
    fold(b, c);
    ++b.draws;
    gfx::fill_rect(d.screen, frect(r, d), c, {.layer = d.layer, .ignore_camera = true});
}

void out_outline(const UiBlock& b, const DrawCtx& d, const UiRect& r, Color color) {
    const Color c = fade(color, d.opacity);
    fold_head(b, 2, d);
    fold(b, r.x + d.dx);
    fold(b, r.y + d.dy);
    fold(b, r.w);
    fold(b, r.h);
    fold(b, c);
    ++b.draws;
    gfx::draw_rect(d.screen, frect(r, d), c, {.layer = d.layer, .ignore_camera = true});
}

void out_sprite(const UiBlock& b, const DrawCtx& d, int slot, const Region& src, int x, int y, float sx, float sy,
                Color tint) {
    if (slot < 0 || slot >= kMaxUiTextures || b.textures[slot] == nullptr) return;
    const Color c = fade(tint, d.opacity);
    fold_head(b, 3, d);
    fold(b, slot);
    fold(b, src.x);
    fold(b, src.y);
    fold(b, src.w);
    fold(b, src.h);
    fold(b, x + d.dx);
    fold(b, y + d.dy);
    fold(b, sx);
    fold(b, sy);
    fold(b, c);
    ++b.draws;
    gfx::draw(d.screen, *b.textures[slot], {static_cast<float>(x + d.dx), static_cast<float>(y + d.dy)},
              {.region = src, .scale = {sx, sy}, .tint = c, .layer = d.layer, .ignore_camera = true});
}

void out_run(const UiBlock& b, const DrawCtx& d, int x, int y, const char* utf8, int length, const UiTextLook& look,
             Color color, bool ellipsis) {
    char run[kUiRunBytes + 8];
    int n = length < 0 ? 0 : (length > kUiRunBytes ? kUiRunBytes : length);
    std::memcpy(run, utf8, static_cast<std::size_t>(n));
    if (ellipsis) {
        std::memcpy(run + n, "...", 3);
        n += 3;
    }
    if (n == 0) return;
    std::memset(run + n, 0, 4); // the NUL, and the zero bytes of the hash's last word
    const Color c = fade(color, d.opacity);
    const Color shadow = fade(look.shadow_color, d.opacity);
    fold_head(b, 4, d);
    fold(b, x + d.dx);
    fold(b, y + d.dy);
    fold(b, c);
    fold(b, static_cast<int>(look.scale) | (look.shadow ? 0x100 : 0));
    if (look.shadow) fold(b, shadow);
    // the run's bytes in little-endian words, the last one zero-padded: byte by byte, the same FNV-1a sequence
    std::uint32_t h = b.draw_hash;
    const int padded = (n + 3) & ~3;
    for (int i = 0; i < padded; ++i) {
        h ^= static_cast<unsigned char>(run[i]);
        h *= 16777619u;
    }
    b.draw_hash = h;
    // glyphs as gfx counts them (ASCII inline; other bytes decoded exactly as gfx does)
    int glyphs = 0;
    for (const char* p = run; *p != 0;) {
        const auto ch = static_cast<unsigned char>(*p);
        if (ch < 0x80) {
            glyphs += ch != ' ' && ch != '\n' && ch != '\r' ? 1 : 0;
            ++p;
        } else {
            glyphs += seam_next_slow(p) != 0xA0 ? 1 : 0;
        }
    }
    b.draws += look.shadow ? 2 * glyphs : glyphs;
    gfx::draw_text(d.screen, {static_cast<float>(x + d.dx), static_cast<float>(y + d.dy)}, run,
                   {.color = c, .scale = look.scale, .shadow = look.shadow, .shadow_color = shadow,
                    .layer = d.layer, .ignore_camera = true});
}

void out_background(const UiBlock& b, const DrawCtx& d, const UiRect& r, const UiBackground& bg) {
    if (r.w <= 0 || r.h <= 0) return;
    switch (bg.kind) {
    case UiBackgroundKind::None: return;
    case UiBackgroundKind::Solid:
        out_fill(b, d, r, bg.color);
        if (bg.border) out_outline(b, d, r, bg.border_color);
        return;
    case UiBackgroundKind::NineSlice: {
        SlicePart parts[9];
        const int n = nine_slice(r, bg.region, bg.left, bg.top, bg.right, bg.bottom, parts);
        for (int i = 0; i < n; ++i) {
            const SlicePart& p = parts[i];
            out_sprite(b, d, bg.texture, {p.sx, p.sy, p.sw, p.sh}, p.dx, p.dy, p.scale_x, p.scale_y, bg.color);
        }
        return;
    }
    }
}

} // namespace nnh::detail
