#pragma once
// Where the parts of a widget lie (spec 028 FR-6, FR-19, FR-26), shared by drawing and touch so both agree: the
// content rectangle, a toggle's box, a slider's label, track, knob and value text, a choice's arrows and option area.

#include "ui/ui_block.hpp"

namespace nnh::detail {

inline constexpr int kUiToggleBox = 10;
inline constexpr int kUiGap = 4;

/// The rectangle without the padding.
UiRect content_rect(const UiBlock& b, int s);

struct SliderParts {
    UiRect label{}, track{}, knob{}, value{}; ///< value: where its text ends (right-aligned)
};
SliderParts slider_parts(UiBlock& b, int s);
/// The value a touch at x on the track gives (stepped, clamped, halves up).
int slider_value_at(const UiBlock& b, int s, const UiRect& track, int x);

struct ChoiceParts {
    UiRect label{}, left{}, option{}, right{};
};
ChoiceParts choice_parts(UiBlock& b, int s);

/// The width (pixels) of the widest value text of slider `s` (min or max), 0 when not shown.
int slider_value_width(UiBlock& b, int s);

} // namespace nnh::detail
