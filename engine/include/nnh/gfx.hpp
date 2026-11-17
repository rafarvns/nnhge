#pragma once
/// @file nnh/gfx.hpp
/// Screens, colors and per-frame graphics state. See docs/api/gfx.md.

#include <cstdint>

namespace nnh {

/// RGBA color, 8 bits per channel. Alpha 255 is opaque.
struct Color {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;
};

/// Builds an opaque color.
/// @code
/// nnh::Color sky = nnh::rgb(32, 48, 96);
/// @endcode
constexpr Color rgb(std::uint8_t r, std::uint8_t g, std::uint8_t b) { return Color{r, g, b, 255}; }

/// Builds a color with explicit alpha.
constexpr Color rgba(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) { return Color{r, g, b, a}; }

/// Common colors.
namespace colors {
inline constexpr Color black{0, 0, 0, 255};
inline constexpr Color white{255, 255, 255, 255};
inline constexpr Color red{255, 0, 0, 255};
inline constexpr Color green{0, 255, 0, 255};
inline constexpr Color blue{0, 0, 255, 255};
inline constexpr Color yellow{255, 255, 0, 255};
inline constexpr Color gray{128, 128, 128, 255};
} // namespace colors

/// The two 3DS screens. On platforms with one screen, both map to regions of it.
enum class Screen : std::uint8_t {
    Top,    ///< 400x240, the stereoscopic screen.
    Bottom, ///< 320x240, the touch screen.
};

/// Width of a screen in pixels (Top 400, Bottom 320).
constexpr int screen_width(Screen screen) { return screen == Screen::Top ? 400 : 320; }

/// Height of a screen in pixels (always 240).
constexpr int screen_height(Screen) { return 240; }

namespace gfx {

/// Sets the color a screen is cleared to at the start of every frame. Default: black.
/// Takes effect from the next call to nnh::frame(). Cheap; safe to call every frame.
/// @code
/// nnh::gfx::set_clear_color(nnh::Screen::Top, nnh::rgb(32, 48, 96));
/// @endcode
void set_clear_color(Screen screen, Color color);

} // namespace gfx
} // namespace nnh
