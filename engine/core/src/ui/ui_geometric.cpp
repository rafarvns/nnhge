// The geometric rule (ui_geometric.hpp).

#include "ui/ui_geometric.hpp"

namespace nnh::detail {

bool geo_candidate(const UiRect& from, const UiRect& to, UiDir d, int order, GeoKey& key) {
    const bool vertical = d == UiDir::Up || d == UiDir::Down;
    int gap = 0;
    switch (d) {
    case UiDir::Down: gap = to.y - (from.y + from.h); break;
    case UiDir::Up: gap = from.y - (to.y + to.h); break;
    case UiDir::Right: gap = to.x - (from.x + from.w); break;
    case UiDir::Left: gap = from.x - (to.x + to.w); break;
    }
    if (gap < 0) return false;
    key.gap = gap;
    key.overlap = vertical ? (to.x < from.x + from.w && to.x + to.w > from.x)
                           : (to.y < from.y + from.h && to.y + to.h > from.y);
    const int c = vertical ? (2 * to.x + to.w) - (2 * from.x + from.w) : (2 * to.y + to.h) - (2 * from.y + from.h);
    key.cross = c < 0 ? -c : c;
    key.order = order;
    return true;
}

bool geo_better(const GeoKey& a, const GeoKey& b) {
    if (a.overlap != b.overlap) return a.overlap;
    if (a.gap != b.gap) return a.gap < b.gap;
    if (a.cross != b.cross) return a.cross < b.cross;
    return a.order < b.order;
}

} // namespace nnh::detail
