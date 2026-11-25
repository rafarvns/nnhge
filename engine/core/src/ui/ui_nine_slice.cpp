// 9-slice geometry (ui_nine_slice.hpp).

#include "ui/ui_nine_slice.hpp"

namespace nnh::detail {
namespace {

/// Insets `a` and `b` scaled down proportionally to fit `size` (a gets the floor, b the rest).
void fit(int size, int& a, int& b) {
    if (a < 0) a = 0;
    if (b < 0) b = 0;
    if (a + b <= size) return;
    const int sum = a + b;
    a = sum > 0 ? size * a / sum : 0;
    b = size - a;
}

} // namespace

int nine_slice(const UiRect& dst, const Region& src, int left, int top, int right, int bottom, SlicePart* out) {
    if (dst.w <= 0 || dst.h <= 0 || src.w <= 0 || src.h <= 0) return 0;
    int sl = left, sr = right, st = top, sb = bottom; // texture insets
    fit(src.w, sl, sr);
    fit(src.h, st, sb);
    int dl = sl, dr = sr, dt = st, db = sb; // screen insets
    fit(dst.w, dl, dr);
    fit(dst.h, dt, db);
    const int dxs[3] = {dst.x, dst.x + dl, dst.x + dst.w - dr};
    const int dws[3] = {dl, dst.w - dl - dr, dr};
    const int dys[3] = {dst.y, dst.y + dt, dst.y + dst.h - db};
    const int dhs[3] = {dt, dst.h - dt - db, db};
    const int sxs[3] = {src.x, src.x + sl, src.x + src.w - sr};
    const int sws[3] = {sl, src.w - sl - sr, sr};
    const int sys[3] = {src.y, src.y + st, src.y + src.h - sb};
    const int shs[3] = {st, src.h - st - sb, sb};
    int n = 0;
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            if (dws[col] <= 0 || dhs[row] <= 0 || sws[col] <= 0 || shs[row] <= 0) continue;
            SlicePart& p = out[n++];
            p.dx = dxs[col];
            p.dy = dys[row];
            p.dw = dws[col];
            p.dh = dhs[row];
            p.sx = sxs[col];
            p.sy = sys[row];
            p.sw = sws[col];
            p.sh = shs[row];
            p.scale_x = static_cast<float>(p.dw) / static_cast<float>(p.sw);
            p.scale_y = static_cast<float>(p.dh) / static_cast<float>(p.sh);
        }
    }
    return n;
}

} // namespace nnh::detail
