#pragma once
/// @file nnh/platform/video.hpp
/// Platform contract — frames and drawing. Implemented by every platform library, called only by
/// nnh::core. Not for games. See docs/architecture/platforms.md.

#include <cstddef>
#include <cstdint>

#include "nnh/gfx.hpp"
#include "nnh/math.hpp"

namespace nnh::platform {

/// A texture as the platform knows it (0 = none). See docs/architecture/rendering.md.
using TextureHandle = std::uintptr_t;

/// One 2D draw of a pass, ready for the GPU: sorted, camera applied, positions in whole screen pixels.
struct DrawItem2D {
    enum class Kind : std::uint8_t { Sprite, FillRect, RectOutline, Line };
    Kind kind = Kind::Sprite;
    std::uint8_t layer = 0; ///< 0..15; kept for stereoscopic depth later (rendering.md)
    bool flip_x = false;    ///< Sprite: mirror the image inside its destination rectangle
    bool flip_y = false;
    TextureHandle texture = 0; ///< Sprite
    Region region{};           ///< Sprite: source pixels in the texture (never empty here)
    /// Sprite: (x, y) = where the pivot lands, (w, h) = destination size.
    /// FillRect/RectOutline: top-left and size. Line: from (x, y) to (w, h).
    float x = 0;
    float y = 0;
    float w = 0;
    float h = 0;
    float pivot_x = 0; ///< Sprite: pivot in destination pixels from the (unmirrored) destination's top-left
    float pivot_y = 0;
    float rotation = 0; ///< Sprite: radians, clockwise on screen, around the pivot
    Color color{};      ///< Sprite: tint (white = unchanged), alpha = opacity. Shapes: the color.
};

/// What the platform reports about a texture it created.
struct TextureInfo {
    TextureHandle handle = 0;
    int width = 0;
    int height = 0;
    std::size_t bytes = 0; ///< GPU memory it uses (core accounts it in the Linear region)
};

/// Decodes an image embedded by the build in this platform's format (3DS: tex3ds .t3x, PC: PNG) into a
/// GPU texture with nearest filtering. Load time only (may allocate). @return false (and `out.handle`
/// 0) if the data can't be decoded or there is no memory; set last_error() then.
bool texture_create(const void* data, std::size_t size, TextureInfo& out);

/// Creates a texture from RGBA8 pixels in memory: row-major, top row first, 4 bytes per pixel (R, G, B,
/// A), sides powers of two from 8 to 1024; nearest filtering. Load time only (may allocate). Used by the
/// engine for the built-in font (spec 005). @return false (and `out.handle` 0) if out of memory.
bool texture_create_rgba(const void* pixels, int width, int height, TextureInfo& out);

/// Frees a texture from texture_create() or texture_create_rgba(). Core calls it only when no submitted frame uses it anymore.
void texture_destroy(TextureHandle handle);

/// Draws one 2D pass on `screen`: `items` are already sorted, camera-adjusted and in whole pixels
/// (see DrawItem2D). Called by core at the end of the frame, between begin_frame() and end_frame(),
/// possibly several times per screen (passes). Draws are clipped to the screen. Must not allocate.
void draw_2d(Screen screen, const DrawItem2D* items, int count);

/// Starts a frame with both screens cleared to the given colors. Called once per nnh::frame(), after
/// poll_events() returned true. Drawing functions are valid until end_frame(). Must not allocate.
void begin_frame(Color top_clear, Color bottom_clear);

/// Submits the frame and waits until the next 60 Hz frame slot (vsync). Called exactly once after each
/// begin_frame(). Must not allocate.
void end_frame();

// Text (game and debug) is drawn by core with the built-in font through texture_create_rgba() and
// draw_2d() (spec 005): a platform implements no text or font functions.

} // namespace nnh::platform
