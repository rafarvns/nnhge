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


} // namespace nnh::detail
