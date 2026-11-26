// 9-slice geometry (spec 028 T1.6, FR-26): nine destination and source rectangles, scales dst / src, parts of size 0
// skipped, insets clamped proportionally when the rectangle is smaller than their sum.

#include <cmath>

#include "test.hpp"
#include "ui/ui_nine_slice.hpp"

using nnh::Region;
using nnh::UiRect;
using nnh::detail::nine_slice;
using nnh::detail::SlicePart;

namespace {

/// Every part lies inside `dst`, parts don't overlap and cover it exactly (area and per-pixel count on a coarse grid).
bool tiles_exactly(const UiRect& dst, const SlicePart* parts, int n) {
    long area = 0;
    for (int i = 0; i < n; ++i) {
        const SlicePart& p = parts[i];
        if (p.dx < dst.x || p.dy < dst.y || p.dx + p.dw > dst.x + dst.w || p.dy + p.dh > dst.y + dst.h) return false;
        area += static_cast<long>(p.dw) * p.dh;
    }
    if (area != static_cast<long>(dst.w) * dst.h) return false;
    for (int y = dst.y; y < dst.y + dst.h; y += 3) {
        for (int x = dst.x; x < dst.x + dst.w; x += 3) {
            int hits = 0;
            for (int i = 0; i < n; ++i) {
                const SlicePart& p = parts[i];
                if (x >= p.dx && x < p.dx + p.dw && y >= p.dy && y < p.dy + p.dh) ++hits;
            }
            if (hits != 1) return false;
        }
    }
    return true;
}

/// Each part's far edge, computed as gfx does (pos + src × scale), lands within 2^-10 of an integer.
bool edges_exact(const SlicePart* parts, int n) {
    for (int i = 0; i < n; ++i) {
        const SlicePart& p = parts[i];
        const float right = static_cast<float>(p.dx) + static_cast<float>(p.sw) * p.scale_x;
        const float bottom = static_cast<float>(p.dy) + static_cast<float>(p.sh) * p.scale_y;
        if (std::fabs(right - static_cast<float>(p.dx + p.dw)) > 1.0f / 1024) return false;
        if (std::fabs(bottom - static_cast<float>(p.dy + p.dh)) > 1.0f / 1024) return false;
    }
    return true;
}

} // namespace

NNH_TEST(ui_nine_slice_parts_of_a_48_by_32_frame) {
    SlicePart parts[9];
    const int n = nine_slice({10, 20, 100, 40}, Region{0, 0, 24, 24}, 8, 6, 4, 2, parts);
    CHECK(n == 9);
    // top-left corner: texture 8x6 at (0,0), screen (10,20) 8x6
    CHECK(parts[0].dx == 10 && parts[0].dy == 20 && parts[0].dw == 8 && parts[0].dh == 6);
    CHECK(parts[0].sx == 0 && parts[0].sy == 0 && parts[0].sw == 8 && parts[0].sh == 6);
    // top edge: texture x 8..20 (12 wide), screen 88 wide
    CHECK(parts[1].dx == 18 && parts[1].dw == 88 && parts[1].sx == 8 && parts[1].sw == 12);
    // top-right corner
    CHECK(parts[2].dx == 106 && parts[2].dw == 4 && parts[2].sx == 20 && parts[2].sw == 4);
    // centre: screen (18,26) 88x32, texture (8,6) 12x16
    CHECK(parts[4].dx == 18 && parts[4].dy == 26 && parts[4].dw == 88 && parts[4].dh == 32);
    CHECK(parts[4].sx == 8 && parts[4].sy == 6 && parts[4].sw == 12 && parts[4].sh == 16);
    // bottom-right corner
    CHECK(parts[8].dx == 106 && parts[8].dy == 58 && parts[8].dw == 4 && parts[8].dh == 2);
    CHECK(parts[8].sx == 20 && parts[8].sy == 22);
    CHECK(parts[4].scale_x == 88.0f / 12.0f);
}

NNH_TEST(ui_nine_slice_every_size_from_the_insets_sum_to_400_tiles_without_seams) {
    const Region regions[3] = {{0, 0, 16, 16}, {16, 32, 24, 12}, {5, 7, 9, 30}};
    const int insets[3][4] = {{4, 4, 4, 4}, {6, 3, 5, 2}, {2, 10, 3, 9}};
    bool ok = true;
    for (int k = 0; k < 3; ++k) {
        const int l = insets[k][0], t = insets[k][1], r = insets[k][2], b = insets[k][3];
        for (int w = l + r; w <= 400 && ok; w += 7) {
            for (int h = t + b; h <= 240 && ok; h += 13) {
                SlicePart parts[9];
                const UiRect dst{3, 5, w, h};
                const int n = nine_slice(dst, regions[k], l, t, r, b, parts);
                ok = tiles_exactly(dst, parts, n) && edges_exact(parts, n);
            }
        }
    }
    CHECK(ok);
}

NNH_TEST(ui_nine_slice_parts_of_size_zero_are_skipped) {
    SlicePart parts[9];
    // width = insets' sum: no centre column
    CHECK(nine_slice({0, 0, 8, 20}, Region{0, 0, 16, 16}, 4, 4, 4, 4, parts) == 6);
    // no insets at all: only the centre
    CHECK(nine_slice({0, 0, 30, 20}, Region{0, 0, 16, 16}, 0, 0, 0, 0, parts) == 1);
    CHECK(parts[0].dw == 30 && parts[0].sw == 16);
    // an empty destination: nothing
    CHECK(nine_slice({0, 0, 0, 20}, Region{0, 0, 16, 16}, 4, 4, 4, 4, parts) == 0);
}

NNH_TEST(ui_nine_slice_insets_are_clamped_proportionally_below_their_sum) {
    SlicePart parts[9];
    // 6 wide for insets 6 + 2: left 6*6/8 = 4, right 2; 3 tall for 4 + 4: top 1 (3*4/8), bottom 2
    const int n = nine_slice({0, 0, 6, 3}, Region{0, 0, 16, 16}, 6, 4, 2, 4, parts);
    CHECK(n == 4);
    CHECK(parts[0].dw == 4 && parts[0].dh == 1 && parts[0].sw == 6 && parts[0].sh == 4);
    CHECK(parts[1].dx == 4 && parts[1].dw == 2 && parts[1].sx == 14);
    CHECK(parts[2].dy == 1 && parts[2].dh == 2 && parts[2].sy == 12);
    bool ok = true;
    for (int w = 1; w < 8; ++w)
        for (int h = 1; h < 8; ++h) {
            const int m = nine_slice({0, 0, w, h}, Region{0, 0, 16, 16}, 6, 4, 2, 4, parts);
            ok = ok && tiles_exactly({0, 0, w, h}, parts, m) && edges_exact(parts, m);
        }
    CHECK(ok);
}
