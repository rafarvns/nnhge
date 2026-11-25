#pragma once
// Anchors (spec 028 FR-10): where a root's or stack's child goes. Per axis, a point of the parent (start, centre
// rounded down, end) meets the child's same point, plus the offset; or the child fills the parent between margins.
// Pure and integer-only; unit tested in tests/ui/test_anchor.cpp.

#include "nnh/ui.hpp"

namespace nnh::detail {

/// The child's rectangle for a `w` x `h` child placed by `place` in `parent`.
UiRect anchor_place(const UiRect& parent, int w, int h, const UiPlace& place);

} // namespace nnh::detail
