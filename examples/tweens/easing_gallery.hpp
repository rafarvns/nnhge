#pragma once
// The top screen of the showcase: the 19 easing curves, each plotted from nnh::ease() with a marker moving
// along it. `phase` (0..1) is animated by the showcase's own tween set, so the markers freeze with the scene.

void draw_easing_gallery(float phase);
