#pragma once
// The frame's recorded 2D draws (docs/architecture/rendering.md). Pure logic; unit tested in
// tests/gfx/test_draw_list.cpp.
//
// Draw calls record commands during the frame; at its end, build() turns the commands of one screen
// into a pass: camera applied, positions rounded to whole pixels, textures resolved, ordered by layer
// then call. build() doesn't consume the list, so a pass can be built twice (stereo: once per eye).

#include <cstdint>

#include "gfx/texture_table.hpp"
#include "nnh/gfx.hpp"
#include "nnh/math.hpp"
#include "nnh/platform/video.hpp"

namespace nnh::detail {

inline constexpr int kLayerCount = 16;

struct DrawCommand {
    platform::DrawItem2D::Kind kind = platform::DrawItem2D::Kind::Sprite;
    Screen screen = Screen::Top;
    std::uint8_t layer = 0; ///< clamped to 0..15 by record()
    bool ignore_camera = false;
    bool flip_x = false;
    bool flip_y = false;
    std::int16_t tile_batch = -1; ///< >= 0: a tile batch (gfx/tile_batches.hpp), not a 2D item
    int texture_slot = -1; ///< Sprite: texture handle
    std::uint32_t texture_generation = 0;
    Region region{};      ///< Sprite: empty = whole texture
    Vec2 position{};      ///< Sprite: where the pivot lands. Shapes: top-left / line start
    Vec2 second{};        ///< FillRect/RectOutline: size. Line: end point
    Vec2 pivot{};         ///< Sprite: in region pixels
    Vec2 scale{1, 1};     ///< Sprite: negative = flipped
    float rotation = 0;   ///< Sprite: radians
    Color color{};        ///< tint (alpha already includes the opacity) or shape color
};

/// Where a tile batch sits in a pass: draw items [0, at) before it, then the batch.
struct TileSplit {
    int at = 0;
    int batch = 0;
};

class DrawList {
public:
    struct BuildResult {
        int items = 0;  ///< written to `out`
        int stale = 0;  ///< sprites skipped because their texture was destroyed
        int splits = 0; ///< tile batches written to `splits`, in drawing order
    };

    /// Uses `storage` and `order` (both `capacity` long, capacity <= 65535) and clears the list.
    void reset(DrawCommand* storage, std::uint16_t* order, int capacity);

    /// Forgets this frame's commands and drop count.
    void clear();

    /// Records a command (layer clamped to 0..15). @return false (and counts a drop) when full.
    bool record(const DrawCommand& command);

    int count() const { return count_; }
    int dropped() const { return dropped_; }
    int capacity() const { return capacity_; }

    /// Writes the pass for `screen` to `out` (at least capacity() long). Tile batch commands make no item: each
    /// becomes a split point in `splits` (at most `split_capacity`; without storage they are skipped).
    BuildResult build(Screen screen, Vec2 camera, const TextureTable& textures, platform::DrawItem2D* out,
                      int out_capacity, TileSplit* splits = nullptr, int split_capacity = 0) const;

private:
    DrawCommand* storage_ = nullptr;
    std::uint16_t* order_ = nullptr; // scratch for the layer sort
    int capacity_ = 0;
    int count_ = 0;
    int dropped_ = 0;
};

} // namespace nnh::detail
