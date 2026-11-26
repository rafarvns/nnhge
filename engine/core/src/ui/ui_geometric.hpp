#pragma once
// The geometric navigation rule (spec 028 FR-16 step 4, D48): a candidate lies entirely beyond the focused rectangle's
// edge in the direction; candidates overlapping it on the cross axis come first; then the smallest gap between the
// edges; then the smallest distance between centres across (doubled, so integers); then pre-order. Pure; unit tested
// in tests/ui/test_geometric.cpp.

#include "nnh/ui.hpp"

namespace nnh::detail {

struct GeoKey {
    bool overlap = false;
    int gap = 0;
    int cross = 0;
    int order = 0;
};

/// True when `to` qualifies from `from` in direction `d`; `key` gets its ranking values.
bool geo_candidate(const UiRect& from, const UiRect& to, UiDir d, int order, GeoKey& key);
/// `a` ranks before `b`.
bool geo_better(const GeoKey& a, const GeoKey& b);

} // namespace nnh::detail
