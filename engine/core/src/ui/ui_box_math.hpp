#pragma once
// Box arithmetic (spec 028 FR-9, D48): main-axis sizes of a box's children by integer stretch ratios (Godot's size
// flags) and cross-axis alignment. Pure and integer-only; unit tested in tests/ui/test_box_math.cpp.
//
// Distribution: children that don't expand keep `min`. The expanding ones share S = available − Σ non-expanding mins:
// a child's share is S × stretch / Σ stretch (floor); one whose minimum exceeds its share is fixed at its minimum and
// the rest recomputed, until none changes. The remainder pixels go one each to the expanding children that aren't
// fixed, first child first. Nothing is shrunk below its minimum: the sum may exceed `available` (overflow).

#include "nnh/ui.hpp"

namespace nnh::detail {

struct BoxItem {
    int min = 0;
    int stretch = 1;     ///< clamped to 1..255
    bool expand = false;
    bool fixed = false;  ///< out: an expanding child held at its minimum
    int size = 0;        ///< out
};

/// Distributes over the items `each` visits: `each(fn)` calls `fn(BoxItem&)` once per child in order (any storage).
/// @return the sum of the sizes.
template <typename Each>
int box_distribute_each(Each&& each, int available) {
    int space = available;
    int ratio_sum = 0;
    each([&](BoxItem& it) {
        it.stretch = it.stretch < 1 ? 1 : (it.stretch > 255 ? 255 : it.stretch);
        it.fixed = false;
        if (it.expand) {
            ratio_sum += it.stretch;
        } else {
            space -= it.min;
        }
    });
    bool changed = true;
    while (changed && ratio_sum > 0) {
        changed = false;
        const int s = space, r = ratio_sum;
        each([&](BoxItem& it) {
            if (!it.expand || it.fixed) return;
            if (it.min > s * it.stretch / r) {
                it.fixed = true;
                space -= it.min;
                ratio_sum -= it.stretch;
                changed = true;
            }
        });
    }
    int given = 0;
    const int s = space, r = ratio_sum;
    each([&](BoxItem& it) {
        if (!it.expand) {
            it.size = it.min;
        } else if (it.fixed) {
            it.size = it.min;
        } else {
            it.size = s * it.stretch / r;
            given += it.size;
        }
    });
    int left = ratio_sum > 0 ? space - given : 0;
    int total = 0;
    each([&](BoxItem& it) {
        if (it.expand && !it.fixed && left > 0) {
            ++it.size;
            --left;
        }
        total += it.size;
    });
    return total;
}

/// The same over an array.
int box_distribute(BoxItem* items, int count, int available);

struct AxisSpan {
    int pos = 0; ///< relative to the available space's start
    int size = 0;
};

/// A child of `size` on an axis of `available` pixels: Start 0; Center (available − size) >> 1 (rounded down); End
/// available − size; Fill: the whole axis.
AxisSpan box_align(UiAlign align, int available, int size);

} // namespace nnh::detail
