// Anchors (ui_anchor.hpp).

#include "ui/ui_anchor.hpp"

namespace nnh::detail {
namespace {

/// 0 start, 1 centre, 2 end along x and y for each of the nine anchors.
constexpr int kPointX[9] = {0, 1, 2, 0, 1, 2, 0, 1, 2};
constexpr int kPointY[9] = {0, 0, 0, 1, 1, 1, 2, 2, 2};

int point(int size, int which) { return which == 0 ? 0 : (which == 1 ? size >> 1 : size); }

/// One axis: {pos, size}.
void axis(int parent_pos, int parent_size, int size, int which, int offset, bool fill, int margin_end, int& pos,
          int& out_size) {
    if (fill) {
        pos = parent_pos + offset;
        out_size = parent_size - offset - margin_end;
        return;
    }
    pos = parent_pos + point(parent_size, which) - point(size, which) + offset;
    out_size = size;
}

} // namespace

UiRect anchor_place(const UiRect& parent, int w, int h, const UiPlace& place) {
    const int a = static_cast<int>(place.anchor) < 9 ? static_cast<int>(place.anchor) : 0;
    UiRect r;
    axis(parent.x, parent.w, w, kPointX[a], place.x, place.fill_x, place.right, r.x, r.w);
    axis(parent.y, parent.h, h, kPointY[a], place.y, place.fill_y, place.bottom, r.y, r.h);
    return r;
}

} // namespace nnh::detail
