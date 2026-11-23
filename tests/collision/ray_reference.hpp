#pragma once
// An exact, independent reference for the grid walk (spec 019 T3.1): coordinates on a 1/256-px lattice held as
// int64, every cell of the segment's bounding range tested against the open segment (0 < t < 1) with integer
// cross products, the two cells beside every grid corner the segment passes through (0 < t < 1) added, sorted by
// the exact entry ratio (ties at a corner: x-neighbour, y-neighbour, then the crossed cell).

#include <algorithm>
#include <cstdint>
#include <vector>

namespace ray_ref {

using I64 = std::int64_t;

/// A rational t = num / den, den > 0.
struct Q {
    I64 num = 0, den = 1;
};
inline bool less(Q a, Q b) { return a.num * b.den < b.num * a.den; }

struct Lattice { // all in 1/256 px
    I64 ox = 0, oy = 0, cw = 1, ch = 1;
    int width = 0, height = 0;
    bool clip = true; ///< only cells of the grid (outside Empty)
};

struct RefCell {
    int x = 0, y = 0;
    Q t{};
    int rank = 2; ///< 0 x-neighbour of a corner, 1 y-neighbour, 2 crossed
};

inline I64 floor_div(I64 a, I64 b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

/// The open interval of t where p + t·d lies strictly between lo and hi; false when empty for every t.
inline bool interval(I64 p, I64 d, I64 lo, I64 hi, bool& moving, Q& in, Q& out) {
    moving = d != 0;
    if (d == 0) return lo < p && p < hi;
    if (d > 0) {
        in = {lo - p, d};
        out = {hi - p, d};
    } else {
        in = {p - hi, -d};
        out = {p - lo, -d};
    }
    return true;
}

inline std::vector<RefCell> cells(const Lattice& g, I64 px, I64 py, I64 qx, I64 qy) {
    std::vector<RefCell> out;
    const I64 dx = qx - px, dy = qy - py;
    if (dx == 0 && dy == 0) return out;
    int x0 = static_cast<int>(floor_div(std::min(px, qx) - g.ox, g.cw)) - 1;
    int x1 = static_cast<int>(floor_div(std::max(px, qx) - g.ox, g.cw)) + 1;
    int y0 = static_cast<int>(floor_div(std::min(py, qy) - g.oy, g.ch)) - 1;
    int y1 = static_cast<int>(floor_div(std::max(py, qy) - g.oy, g.ch)) + 1;
    const auto keep = [&](int x, int y) { return !g.clip || (x >= 0 && y >= 0 && x < g.width && y < g.height); };
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            if (!keep(x, y)) continue;
            const I64 cx0 = g.ox + x * g.cw, cy0 = g.oy + y * g.ch;
            bool mx = false, my = false;
            Q inx, outx, iny, outy;
            if (!interval(px, dx, cx0, cx0 + g.cw, mx, inx, outx) || !interval(py, dy, cy0, cy0 + g.ch, my, iny, outy)) {
                continue;
            }
            Q enter{0, 1}, exit{1, 1};
            if (mx && less(enter, inx)) enter = inx;
            if (my && less(enter, iny)) enter = iny;
            if (mx && less(outx, exit)) exit = outx;
            if (my && less(outy, exit)) exit = outy;
            if (less(enter, exit)) out.push_back({x, y, enter, 2});
        }
    }
    if (dx != 0 && dy != 0) { // corners passed through
        const int sx = dx > 0 ? 1 : -1, sy = dy > 0 ? 1 : -1;
        for (int j = y0; j <= y1 + 1; ++j) {
            for (int i = x0; i <= x1 + 1; ++i) {
                const I64 cx = g.ox + i * g.cw, cy = g.oy + j * g.ch;
                if ((cx - px) * dy != (cy - py) * dx) continue;
                const Q t = dx > 0 ? Q{cx - px, dx} : Q{px - cx, -dx};
                if (!(less({0, 1}, t) && less(t, {1, 1}))) continue;
                const int xn_x = sx > 0 ? i : i - 1, xn_y = sy > 0 ? j - 1 : j;
                const int yn_x = sx > 0 ? i - 1 : i, yn_y = sy > 0 ? j : j - 1;
                if (keep(xn_x, xn_y)) out.push_back({xn_x, xn_y, t, 0});
                if (keep(yn_x, yn_y)) out.push_back({yn_x, yn_y, t, 1});
            }
        }
    }
    std::stable_sort(out.begin(), out.end(), [](const RefCell& a, const RefCell& b) {
        if (less(a.t, b.t)) return true;
        if (less(b.t, a.t)) return false;
        return a.rank < b.rank;
    });
    return out;
}

/// The fraction the engine must report for `t`: the exact ratio rounded once.
inline float fraction(Q t) { return static_cast<float>(static_cast<double>(t.num) / static_cast<double>(t.den)); }

} // namespace ray_ref
