// Packing glyph bitmaps into pages and baking outlines (glyph_pack.hpp).

#include "glyph_pack.hpp"

#include <algorithm>

namespace nnh::assetc {
namespace {

struct Item {
    int w = 0, h = 0;
    char32_t cp = 0;
    bool outline = false;
    BakedGlyph* glyph = nullptr;
};

constexpr PageSize kSizes[] = {{64, 64},    {128, 64},   {128, 128},  {256, 128},  {256, 256},
                               {512, 256},  {512, 512},  {1024, 512}, {1024, 1024}};

/// Shelf-packs items [first, end) into a page of `size`: how many fit, in order (positions written when `place`).
std::size_t shelf(std::vector<Item>& items, std::size_t first, PageSize size, int pad, int page, bool place) {
    int x = pad, y = pad, shelf_h = 0;
    std::size_t i = first;
    for (; i < items.size(); ++i) {
        Item& it = items[i];
        if (it.w + 2 * pad > size.w || it.h + 2 * pad > size.h) break;
        if (x + it.w + pad > size.w) {
            x = pad;
            y += shelf_h + pad;
            shelf_h = 0;
        }
        if (y + it.h + pad > size.h) break;
        if (place) {
            if (it.outline) {
                it.glyph->outline_page = page;
                it.glyph->outline_x = x;
                it.glyph->outline_y = y;
            } else {
                it.glyph->page = page;
                it.glyph->x = x;
                it.glyph->y = y;
            }
        }
        x += it.w + pad;
        shelf_h = std::max(shelf_h, it.h);
    }
    return i - first;
}

} // namespace

std::string pack_glyphs(BakedFont& font, std::vector<PageSize>& pages) {
    std::vector<Item> items;
    for (BakedGlyph& g : font.glyphs) {
        if (g.glyph.w > 0 && g.glyph.h > 0) items.push_back({g.glyph.w, g.glyph.h, g.cp, false, &g});
        if (g.outline.w > 0 && g.outline.h > 0) items.push_back({g.outline.w, g.outline.h, g.cp, true, &g});
    }
    std::stable_sort(items.begin(), items.end(), [](const Item& a, const Item& b) {
        if (a.h != b.h) return a.h > b.h;
        if (a.w != b.w) return a.w > b.w;
        if (a.cp != b.cp) return a.cp < b.cp;
        return !a.outline && b.outline;
    });
    pages.clear();
    std::size_t done = 0;
    while (done < items.size() || pages.empty()) {
        PageSize chosen{};
        if (font.page_side > 0) {
            chosen = {font.page_side, font.page_side};
        } else {
            chosen = kSizes[sizeof kSizes / sizeof kSizes[0] - 1];
            for (const PageSize s : kSizes) {
                if (shelf(items, done, s, font.padding, 0, false) == items.size() - done) {
                    chosen = s;
                    break;
                }
            }
        }
        const std::size_t n = shelf(items, done, chosen, font.padding, static_cast<int>(pages.size()), true);
        if (n == 0 && done < items.size()) return "a glyph is larger than a page";
        pages.push_back(chosen);
        done += n;
        if (pages.size() > 8) break;
    }
    if (pages.size() > 8) {
        // how many full pages the whole set needs, for the message
        std::size_t all = 0;
        int needed = 0;
        while (all < items.size()) {
            const std::size_t n = shelf(items, all, {1024, 1024}, font.padding, 0, false);
            if (n == 0) break;
            all += n;
            ++needed;
        }
        return std::to_string(font.glyphs.size()) + " glyphs at " + std::to_string(font.size) + " px need " +
               std::to_string(needed) + " pages of 1024x1024 (limit 8): reduce the size or the character set";
    }
    return "";
}

Bitmap bake_outline(const Bitmap& g, int r) {
    Bitmap o;
    o.w = g.w + 2 * r;
    o.h = g.h + 2 * r;
    o.ox = g.ox - r;
    o.oy = g.oy - r;
    o.pixels.assign(static_cast<std::size_t>(o.w * o.h), 0);
    for (int y = 0; y < o.h; ++y) {
        for (int x = 0; x < o.w; ++x) {
            std::uint8_t best = 0;
            for (int dy = -r; dy <= r; ++dy) {
                for (int dx = -r; dx <= r; ++dx) {
                    if (dx * dx + dy * dy > r * r + r) continue;
                    const int sx = x - r - dx, sy = y - r - dy;
                    if (sx < 0 || sy < 0 || sx >= g.w || sy >= g.h) continue;
                    best = std::max(best, g.pixels[static_cast<std::size_t>(sy * g.w + sx)]);
                }
            }
            o.pixels[static_cast<std::size_t>(y * o.w + x)] = best;
        }
    }
    return o;
}

} // namespace nnh::assetc
