#pragma once
// PC simulator: where the two 3DS screens go in the window (spec 006). Pure and SDL-free, so it is unit
// tested (tests/simulator/test_sim_layout.cpp). Everything is in window (render-output) pixels with
// integer scales, so pixels stay sharp.

#include <cstdint>

namespace nnh::platform::pc {

enum class SimLayout : std::uint8_t { Stacked, SideBySide, LargeTop, TopOnly, BottomOnly };

inline constexpr int kMaxSimScale = 6;  // largest scale chosen with the keys
inline constexpr int kMaxFitScale = 16; // largest scale "fit" may pick (big monitors)

struct SimSize {
    int w = 0;
    int h = 0;
};

/// Where one screen is drawn: top-left corner and integer scale.
struct Placement {
    bool visible = false;
    int x = 0;
    int y = 0;
    int scale = 1;
};

struct LayoutResult {
    Placement top;
    Placement bottom;
    SimSize size; ///< the content's size (both screens)
};

/// Size the layout takes at `scale`: stacked 400s x 480s, side by side 720s x 240s, large top
/// 400s + 320b x 240s with the bottom at b = max(1, s / 2), one screen 400s or 320s x 240s.
SimSize natural_size(SimLayout layout, int scale);

/// The largest scale (1..kMaxFitScale) whose natural size fits the window; 1 if none does.
int fit_scale(SimLayout layout, int window_w, int window_h);

/// The screens' places at `scale`, the content centered in the window.
LayoutResult compute_layout(SimLayout layout, int scale, int window_w, int window_h);

/// Converts a window point to bottom-screen pixels. Returns false if the bottom screen isn't shown, or
/// the point is outside it and `clamp` is false; with `clamp` (a touch being held) the point is clamped
/// to the screen's edges.
bool window_to_bottom(const LayoutResult& layout, float x, float y, bool clamp, float& bottom_x, float& bottom_y);

SimLayout next_layout(SimLayout layout);

/// "stacked", "side", "large", "top", "bottom".
const char* layout_id(SimLayout layout);
/// For the window title: "Stacked", "Side by side", "Large top", "Top only", "Bottom only".
const char* layout_title(SimLayout layout);
/// Parses a layout_id(). @return false (and leaves `out`) if unknown.
bool parse_layout(const char* id, SimLayout& out);

} // namespace nnh::platform::pc
