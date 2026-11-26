#pragma once
/// @file nnh/detail/ui_style.hpp
/// The style structs of <nnh/ui.hpp> (spec 028 FR-26, FR-27; decision D48). Plain aggregates of integers, bools, enums,
/// Color and Region only, so a game can describe a theme in a data file with its own spec 014 data type and convert it
/// (docs/guides/menus.md). Included by <nnh/ui.hpp>; not meant to be included alone.

#include <cstdint>

#include "nnh/ease.hpp"
#include "nnh/gfx.hpp"
#include "nnh/math.hpp"

namespace nnh {

/// The font a style's text uses. Only the built-in font today; the game-fonts spec adds values (FR-24's seam).
enum class UiFont : std::uint8_t { Builtin };

/// What a background is: nothing, a solid colour (with an optional 1-pixel border) or a 9-slice of a texture.
enum class UiBackgroundKind : std::uint8_t { None, Solid, NineSlice };

/// A background (FR-26). NineSlice: `region` of the UI's texture slot `texture` (Ui::set_texture), cut by four insets
/// in texture pixels; corners keep their size, edges and centre stretch, drawn at whole pixels with no seams.
struct UiBackground {
    UiBackgroundKind kind = UiBackgroundKind::None;
    std::uint8_t texture = 0; ///< NineSlice: texture slot 0..7
    bool border = false;      ///< Solid: a 1-pixel outline in border_color
    Color color = colors::white; ///< Solid: the fill; NineSlice: a tint (white = unchanged)
    Color border_color{};
    Region region{};          ///< NineSlice: the frame in the texture
    std::uint8_t left = 0, top = 0, right = 0, bottom = 0; ///< NineSlice insets, texture pixels
};

/// How a widget looks in one state.
struct UiLook {
    UiBackground background{};
    Color text = colors::white;  ///< text colour
    Color tint = colors::white;  ///< multiplies images and icons
};

/// Text settings of a style. `font` is the game-fonts spec's hook; `scale` 1..8 as gfx::draw_text.
struct UiTextLook {
    UiFont font = UiFont::Builtin;
    std::uint8_t scale = 1;
    bool shadow = false;
    Color shadow_color = rgba(0, 0, 0, 160);
};

/// A picture from a texture slot (-1: none).
struct UiImagePart {
    std::int8_t texture = -1;
    Region region{};
};

/// Slider parts: the track (whole length), its filled part (start to the knob) and the knob.
struct UiSliderParts {
    UiBackground track{}, fill{}, knob{};
    std::uint8_t knob_width = 6;
    std::uint8_t track_height = 4;
};

/// Toggle parts: images for off and on (none: a drawn box, filled when on).
struct UiToggleParts {
    UiImagePart off{}, on{};
};

/// Choice parts: arrow images (none: "<" and ">" in text when text_arrows).
struct UiChoiceParts {
    UiImagePart left{}, right{};
    bool text_arrows = true;
};

/// List and grid parts: row backgrounds (normal, focused, disabled) and the scroll bar.
struct UiListParts {
    UiBackground row[3]{};
    UiBackground track{}, thumb{};
    std::uint8_t scroll_width = 4;
    std::uint8_t row_height = 12;
};

/// Progress bar parts: the fill over the state's background.
struct UiProgressParts {
    UiBackground fill{};
};

/// The look of a widget (FR-26): one per kind in a theme, or a custom one (Ui::add_style). Unused parts are ignored by
/// the kinds that don't have them. Plain values: copy and change freely.
struct UiStyle {
    UiLook states[5]{};                      ///< indexed by UiState: Normal, Focused, Pressed, Disabled, Checked
    UiTextLook text{};
    std::uint8_t padding[4] = {4, 2, 4, 2}; ///< left, top, right, bottom
    UiSliderParts slider{};
    UiToggleParts toggle{};
    UiChoiceParts choice{};
    UiListParts list{};
    UiProgressParts progress{};
};

/// Where the focus cursor's image sits around the focused widget.
enum class UiAnchor : std::uint8_t { TopLeft, Top, TopRight, Left, Center, Right, BottomLeft, Bottom, BottomRight };

/// The focus cursor (FR-27): an image placed outside the focused widget at `anchor` (+ x, y), and/or an outline; it
/// glides between widgets over move_frames with `ease` (0: jumps) and may pulse (opacity and x) every pulse_frames.
struct UiCursorStyle {
    UiImagePart image{};
    UiAnchor anchor = UiAnchor::Left;
    std::int8_t x = -2, y = 0;
    bool outline = false;
    Color outline_color = colors::white;
    std::uint8_t move_frames = 6;
    Ease ease = Ease::CubicOut;
    std::uint8_t pulse_frames = 0;      ///< 0: no pulse
    std::uint8_t pulse_opacity = 160;   ///< opacity at the pulse's low point
    std::int8_t pulse_x = 0;            ///< x offset at the pulse's far point
};

/// A theme: the default style of each kind and the cursor. A UI holds it by reference (the game keeps it alive).
struct UiTheme {
    UiStyle label, image, progress, panel, button, toggle, slider, choice, list, grid, scroll_box;
    UiCursorStyle cursor;
};

/// A style of the UI: 0 = the widget kind's theme style; 1.. = a style added with Ui::add_style.
struct UiStyleId {
    std::uint8_t value = 0;
    constexpr bool operator==(const UiStyleId&) const = default;
};

} // namespace nnh
