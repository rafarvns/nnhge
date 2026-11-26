#pragma once
// The example's 9-slice theme (art/ui.png on texture slot 0: framed panels and buttons, a slider with a knob, toggle
// and arrow images, a hand cursor that glides and pulses) next to the engine's default theme (solid colours, no art).
// SELECT switches between them in every UI.

#include <nnh/ui.hpp>

/// The 9-slice theme (built once).
const nnh::UiTheme& nine_slice_theme();

/// Regions of the sheet other code draws with (the HUD's heart, the dialog's frame).
inline constexpr nnh::Region kHeart{48, 24, 12, 12};
