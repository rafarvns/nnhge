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

/// TileQuad2D::orient bits: Tiled's flips. The diagonal flip (swap x and y, square tiles only) is applied
/// first, then the x and y mirrors. The values are a cell's bits 13..15 (core shifts them down once).
inline constexpr std::uint8_t kTileOrientDiagonal = 1;
inline constexpr std::uint8_t kTileOrientFlipY = 2;
inline constexpr std::uint8_t kTileOrientFlipX = 4;
/// Zoom (decision D33): this quad is drawn one pixel wider than TileBatch2D::dst_w / taller than dst_h, so the
/// zoomed tiles of a layer meet without gaps or overlaps. Never set at zoom 1 or at integer zooms.
inline constexpr std::uint8_t kTileOrientWiden = 8;
inline constexpr std::uint8_t kTileOrientTaller = 16;

/// One tile of a batch: the tile_w x tile_h square of the texture at (src_x, src_y), drawn at a whole-pixel
/// position with the batch's destination size dst_w x dst_h (plus one pixel per kTileOrientWiden/Taller bit).
struct TileQuad2D {
    std::int16_t x = 0; ///< destination top-left, screen pixels
    std::int16_t y = 0;
    std::uint16_t src_x = 0; ///< source top-left in the texture, pixels
    std::uint16_t src_y = 0;
    std::uint8_t orient = 0; ///< kTileOrient* bits
};

/// Same-size tiles of one texture, from one map layer (spec 012, decision D26; zoom: D33).
struct TileBatch2D {
    TextureHandle texture = 0;
    int tile_w = 0; ///< source size in texture pixels, 8..64
    int tile_h = 0;
    int dst_w = 0; ///< destination size on screen: floor(tile_w × zoom) — tile_w at zoom 1 (camera zoom, D33)
    int dst_h = 0;
    Color color{};          ///< tint for every tile (white = unchanged), alpha = opacity
    std::uint8_t layer = 0; ///< 0..15, as DrawItem2D
    const TileQuad2D* quads = nullptr;
    int count = 0;
};

/// Draws the quads of `batch` on `screen`, in order, clipped to it; nearest sampling, so whole-pixel
/// positions and exact source rectangles give seamless tiles. Each quad covers exactly
/// {x, y, dst_w + widen, dst_h + taller} (the diagonal flip transposes the source into that rectangle). Called by core between draw_2d() calls of a
/// screen (draw order = call order). Must not allocate, and must be done with `batch.quads` when it returns
/// (core reuses the buffer).
void draw_tiles(Screen screen, const TileBatch2D& batch);

/// Starts a frame with both screens cleared to the given colors. Called once per nnh::frame(), after
/// poll_events() returned true. Drawing functions are valid until end_frame(). Must not allocate.
void begin_frame(Color top_clear, Color bottom_clear);

/// Submits the frame and waits until the next 60 Hz frame slot (vsync). Called exactly once after each
/// begin_frame(). Must not allocate.
void end_frame();

// Text (game and debug) is drawn by core with the built-in font through texture_create_rgba() and
// draw_2d() (spec 005): a platform implements no text or font functions.

static_assert(sizeof(TileQuad2D) <= 12, "tile quads stay compact (core's scratch holds 1600)");

} // namespace nnh::platform
