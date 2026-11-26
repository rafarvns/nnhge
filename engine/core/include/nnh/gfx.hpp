#pragma once
/// @file nnh/gfx.hpp
/// Screens, colors, 2D drawing (sprites, shapes) and cameras. See docs/api/gfx.md and
/// docs/architecture/rendering.md.

#include <cstddef>
#include <cstdint>

#include "nnh/math.hpp"

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

/// How a texture is drawn by gfx::draw(). Every field is optional:
/// @code
/// nnh::gfx::draw(nnh::Screen::Top, hero, pos, {.region = {16 * frame, 0, 16, 16}, .flip_x = facing_left});
/// @endcode
struct DrawOptions {
    Region region{};            ///< pixels of the texture to draw (a sprite-sheet frame); empty = all
    Vec2 pivot{};               ///< point of the region placed at `position`; rotation and scale use it
    float rotation = 0;         ///< radians, clockwise on screen
    Vec2 scale{1, 1};           ///< a negative scale mirrors, like flip_x/flip_y
    bool flip_x = false;        ///< mirror horizontally
    bool flip_y = false;        ///< mirror vertically
    Color tint = colors::white; ///< multiplies the texture's colors (white = unchanged)
    float opacity = 1;          ///< 0 (invisible) to 1, multiplies the tint's alpha
    int layer = 0;              ///< 0..15: higher layers draw on top; same layer = call order
    bool ignore_camera = false; ///< draw in screen pixels (HUD)
};

/// Layer and camera for shapes.
struct ShapeOptions {
    int layer = 0;              ///< 0..15
    bool ignore_camera = false; ///< draw in screen pixels (HUD)
};

/// Camera2D::zoom range (decision D33).
inline constexpr float kMinCameraZoom = 0.5f;
inline constexpr float kMaxCameraZoom = 4.0f;

/// The view of a screen: `position` is the world point shown at the screen's top-left corner (at every zoom).
/// A camera-relative draw lands at `(world − position) × zoom` screen pixels: sprites, text and rectangles grow
/// `zoom` times (outlines and lines stay one pixel thick), tile layers stay seamless (tile edges snapped to whole
/// pixels). Draws with `ignore_camera`, debug text and the overlay are never zoomed. Zoom 1 draws exactly as with no
/// zoom; integer zooms are pixel-exact, fractional ones give uneven pixel sizes. A 3D camera will be a separate type.
/// To zoom about a point other than the top-left, move `position` too (nnh::CameraRig zooms about the view's centre).
struct Camera2D {
    Vec2 position{};
    float zoom = 1; ///< kMinCameraZoom..kMaxCameraZoom; set_camera clamps it (asserts in Debug/Development)
};

/// Horizontal alignment of text, relative to the x of its position, line by line.
enum class TextAlign : std::uint8_t { Left, Center, Right };

/// The built-in font: every character is a cell of kFontCellSize x kFontCellSize pixels (at scale 1),
/// lines are kFontLineHeight apart. The same on every system.
inline constexpr int kFontCellSize = 8;
inline constexpr int kFontLineHeight = 10;
/// A game font by handle (spec 029, D49): a font table slot and generation; 0 = the built-in font. See <nnh/font.hpp>.
struct FontRef {
    std::uint32_t id = 0;
    constexpr FontRef() = default; constexpr bool builtin() const { return id == 0; }
    FontRef(const class Font& font); // NOLINT: implicit on purpose ({.font = body}); defined in the font module
};
/// How gfx::draw_text() draws. Every field is optional, e.g. {.scale = 2, .align = nnh::TextAlign::Center}.
struct TextOptions {
    Color color = colors::white;
    int scale = 1;                           ///< 1 to 8: whole pixels, so text stays sharp
    TextAlign align = TextAlign::Left;       ///< relative to the position's x, line by line
    bool shadow = false;                     ///< a drop shadow one (scaled) pixel down-right
    Color shadow_color = rgba(0, 0, 0, 160);
    int layer = 0;                           ///< 0..15, like sprites
    bool ignore_camera = false;              ///< position in screen pixels (HUD)
    FontRef font{};                          ///< a game font (<nnh/font.hpp>); default: the built-in font
    int wrap_width = 0, max_lines = 0;       ///< > 0: break lines at this width (pixels) / at most this many lines (…)
    Color outline_color{0, 0, 0, 0};         ///< alpha > 0: the font's baked outline drawn first in this colour
};
class Texture; struct WorldTransform2D; namespace gfx { // <nnh/texture.hpp>, <nnh/transform.hpp>
/// Draws a texture (or a region of it) with its pivot at `position`, in world pixels (screen pixels
/// minus the screen's camera). Records the draw: it appears when the frame ends, ordered by layer, then
/// call. Only inside a frame; an empty texture draws nothing. No allocation.
void draw(Screen screen, const Texture& texture, Vec2 position, const DrawOptions& options = {});

/// `given` with a world transform merged in (spec 022 FR-13): rotation = the world's + the given one (subtracted under a
/// mirrored world, as a child's), scale = the world's × the given one; region, pivot, flips, tint, opacity, layer and
/// ignore_camera kept. A mirror is a negative scale: centre the pivot so a mirrored sprite stays in place. Pure.
DrawOptions transformed(const WorldTransform2D& world, const DrawOptions& given = {});
/// draw(screen, texture, world.position(), transformed(world, options)): a node's sprite where the hierarchy put it.
void draw(Screen screen, const Texture& texture, const WorldTransform2D& world, const DrawOptions& options = {});

/// Filled rectangle.
void fill_rect(Screen screen, Rect rect, Color color, const ShapeOptions& options = {});

/// Rectangle outline, 1 pixel wide, inside `rect`.
void draw_rect(Screen screen, Rect rect, Color color, const ShapeOptions& options = {});

/// 1-pixel line from `from` to `to`.
void draw_line(Screen screen, Vec2 from, Vec2 to, Color color, const ShapeOptions& options = {});

/// Draws UTF-8 text with the built-in NNHGE font, the first line's top-left at `position` (for
/// TextAlign::Left). '\n' starts a new line. Pixel-identical on every system. Each drawn character (and
/// its shadow) is one draw of AppConfig::gfx.max_draws. Only inside a frame. No allocation.
void draw_text(Screen screen, Vec2 position, const char* utf8, const TextOptions& options = {});

/// Size `utf8` takes when drawn at `scale`: {widest line, lines x kFontLineHeight}, times the scale.
/// The same on every system; callable anytime.
/// @code
/// const nnh::Vec2 size = nnh::gfx::measure_text("Game Over", 2);
/// nnh::gfx::fill_rect(nnh::Screen::Top, {x - 4, y - 4, size.x + 8, size.y + 8}, nnh::rgba(0, 0, 0, 160));
/// @endcode
Vec2 measure_text(const char* utf8, int scale = 1);

/// Sets / reads a screen's camera (both start at (0, 0), zoom 1, at init()). The camera at the end of the frame
/// places the frame's draws. A zoom outside kMinCameraZoom..kMaxCameraZoom asserts in Debug/Development and is
/// clamped (NaN → 1) in Release. Cheap; safe every frame.
void set_camera(Screen screen, Camera2D camera);
Camera2D camera(Screen screen);

/// Capacities and usage, to size AppConfig::gfx.
struct Stats {
    int draws_last_frame = 0;     ///< draws submitted by the last finished frame
    int max_draws = 0;            ///< AppConfig::gfx.max_draws
    int dropped_draws = 0;        ///< draws dropped since init() because the frame was full
    int textures = 0;             ///< textures loaded now
    int max_textures = 0;         ///< AppConfig::gfx.max_textures
    std::size_t texture_bytes = 0; ///< GPU memory of the loaded textures
    int tiles_last_frame = 0;      ///< tile quads drawn by the last finished frame (gfx::draw_tilemap)
    int max_tiles = 0;             ///< AppConfig::gfx.max_tiles
    int dropped_tiles = 0;         ///< tile quads dropped since init() because the frame was over max_tiles
    int particles_last_frame = 0;  ///< particle quads drawn by the last finished frame (ParticleEmitter::draw)
    int max_particles = 0;         ///< AppConfig::gfx.max_particles
    int dropped_particles = 0;     ///< particle quads dropped since init() because the frame was over max_particles
};
Stats stats();

} // namespace gfx

// Spec 029 (D49), declared after everything else so the declarations above keep their lines.

/// A font's integer metrics at scale 1 (spec 029 FR-6): the built-in font is {8, 10, 8, 0}. `line_height` = ascent −
/// descent + line gap; `ascent`: the baseline below a line's top; `descent` ≤ 0.
struct FontMetrics {
    int size = 8;            ///< em size in pixels
    int line_height = 10;
    int ascent = 8;
    int descent = 0;
    int glyphs = 0;          ///< glyphs of the font itself (0: the built-in font)
    int pages = 0;
    std::size_t bytes = 0;   ///< its block and pages (as the console counts them)
    bool outline = false;    ///< it has baked outline glyphs
    bool colour = false;     ///< its pages are RGBA (a pre-coloured BMFont)
};

/// Per code point of a measured text (spec 029 FR-8): where its cell is, after alignment, in pixels (× scale) from the
/// text's position; for cursors and typewriter effects.
struct TextCaret {
    std::int32_t byte = 0;    ///< offset of the code point in the text
    std::int16_t x = 0;       ///< its pen position (left of its cell)
    std::int16_t y = 0;       ///< its line's top
    std::int16_t advance = 0; ///< the pen's move after it (0 for zero-width characters)
    std::int16_t line = 0;    ///< 0-based line
};

namespace gfx {

/// The box `utf8` takes when drawn with `options` (spec 029 FR-8): {widest line, lines × line height} × scale, with the
/// font, wrap width, line limit and ellipsis as draw_text lays them out; every drawn glyph's pixels lie inside it (the
/// shadow and the outline glyphs aside: docs/api/text.md#measuring). The same on every system; callable anytime.
/// No allocation.
Vec2 measure_text(const char* utf8, const TextOptions& options);

/// Writes up to `capacity` carets of `utf8` laid out with `options` (one per code point but '\r' and '\n'; the
/// ellipsis has none) and returns how many it wrote. No allocation.
int measure_chars(const char* utf8, const TextOptions& options, TextCaret* out, int capacity);

/// The metrics of `font` at scale 1 (FontRef{}: the built-in font's). A stale ref: the built-in font's.
FontMetrics font_metrics(FontRef font = {});

} // namespace gfx
} // namespace nnh
