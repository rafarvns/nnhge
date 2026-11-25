// Box arithmetic (ui_box_math.hpp).

#include "ui/ui_box_math.hpp"

namespace nnh::detail {

int box_distribute(BoxItem* items, int count, int available) {
    return box_distribute_each(
        [&](auto&& fn) {
            for (int i = 0; i < count; ++i) fn(items[i]);
        },
        available);
}

AxisSpan box_align(UiAlign align, int available, int size) {
    switch (align) {
    case UiAlign::Start: return {0, size};
    case UiAlign::Center: return {(available - size) >> 1, size};
    case UiAlign::End: return {available - size, size};
    case UiAlign::Fill: break;
    }
    return {0, available};
}

} // namespace nnh::detail
