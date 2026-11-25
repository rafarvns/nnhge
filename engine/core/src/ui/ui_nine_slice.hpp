#pragma once
// 9-slice geometry (spec 028 FR-26): the nine destination and source rectangles of a frame stretched over a widget.
// Corners keep their texture size (clamped proportionally when the widget is smaller than the insets' sum), edges and
// the centre stretch; parts of size 0 are skipped. Each part is drawn with scale = destination / source, so its far
// edge lands within 2^-10 of a whole pixel: no seams. Pure; unit tested in tests/ui/test_nine_slice.cpp.

#include "nnh/math.hpp"
#include "nnh/ui.hpp"

namespace nnh::detail {

struct SlicePart {
    int dx = 0, dy = 0, dw = 0, dh = 0; ///< screen pixels
    int sx = 0, sy = 0, sw = 0, sh = 0; ///< texture pixels
    float scale_x = 1, scale_y = 1;     ///< dw / sw, dh / sh
};

/// Fills `out` (9 entries) in row order (top-left, top, top-right, left, centre, right, bottom-left, ...), skipping
/// empty parts. @return the parts written.
int nine_slice(const UiRect& dst, const Region& src, int left, int top, int right, int bottom, SlicePart* out);

} // namespace nnh::detail
