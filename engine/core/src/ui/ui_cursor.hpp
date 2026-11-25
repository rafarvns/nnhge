#pragma once
// The focus cursor's state readers (ui_cursor.cpp), used by drawing and tests.

#include "ui/ui_block.hpp"

namespace nnh::detail {

struct CursorPulse {
    int opacity = 255; ///< 0..255
    int dx = 0;        ///< pixels
};

/// The pulse at the cursor's current phase.
CursorPulse cursor_pulse(const UiBlock& b);
/// The rectangle the cursor points at: the focused list item, else the focused widget.
UiRect cursor_target_rect(const UiBlock& b, int s);
/// from + (to − from) × eased, rounded once (floor(x + 0.5)).
int cursor_lerp(int from, int to, float eased);

} // namespace nnh::detail
