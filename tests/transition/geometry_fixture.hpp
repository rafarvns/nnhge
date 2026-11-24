#pragma once
// Helpers for the effect geometry tests (spec 023): collect an effect's rectangles, compare them, rasterise them.

#include <cstring>

#include "test.hpp"
#include "transition/effect_geometry.hpp"

namespace geo_test {

using nnh::detail::PixelRect;

struct Rects {
    static constexpr int kMax = 600;
    PixelRect r[kMax];
    int n = 0;
};

inline void collect(void* ctx, const PixelRect& rect) {
    auto* rects = static_cast<Rects*>(ctx);
    if (rects->n < Rects::kMax) rects->r[rects->n] = rect;
    ++rects->n;
}

struct Where {
    bool continued = false;
    bool tall = false;
    int gap = 0;
};

/// The rectangles `effect` draws on `screen` at q.
inline Rects draw(const nnh::transition::Effect& effect, nnh::Screen screen, int q, Where where = {}) {
    const nnh::detail::EffectArea area{where.tall, where.gap};
    const nnh::detail::ResolvedEffect fx = nnh::detail::resolve_effect(effect, screen, area, where.continued);
    Rects out;
    nnh::detail::draw_screen_effect(fx, screen, area, q, collect, &out);
    return out;
}

inline bool is(const PixelRect& r, int x, int y, int w, int h) {
    return r.x == x && r.y == y && r.w == w && r.h == h;
}

/// A screen as a byte map: how many rectangles cover each pixel.
struct Raster {
    int w = 400;
    int h = 240;
    unsigned char px[400 * 240] = {};
    bool outside = false; ///< a rectangle left the screen

    explicit Raster(nnh::Screen screen) : w(nnh::screen_width(screen)) {}
    void add(const Rects& rects) {
        for (int i = 0; i < rects.n && i < Rects::kMax; ++i) {
            const PixelRect& r = rects.r[i];
            if (r.w <= 0 || r.h <= 0 || r.x < 0 || r.y < 0 || r.x + r.w > w || r.y + r.h > h) {
                outside = true;
                continue;
            }
            for (int y = r.y; y < r.y + r.h; ++y) {
                for (int x = r.x; x < r.x + r.w; ++x) ++px[y * 400 + x];
            }
        }
    }
    int covered() const {
        int n = 0;
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) n += px[y * 400 + x] > 0 ? 1 : 0;
        }
        return n;
    }
    bool overlaps() const {
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                if (px[y * 400 + x] > 1) return true;
            }
        }
        return false;
    }
    bool at(int x, int y) const { return px[y * 400 + x] > 0; }
};

} // namespace geo_test
