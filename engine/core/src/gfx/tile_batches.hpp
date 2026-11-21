#pragma once
// The frame's tile batches (spec 012, decision D26). Pure logic over caller-provided storage; unit tested in
// tests/gfx/test_tile_batches.cpp.
//
// gfx knows a tile batch only as {expand function, source}: it never includes the tilemap module, so any grid
// could feed it. draw_tilemap records one batch per (map layer, tileset) plus a draw-list command that keeps
// its place among the sprites; at submit, expand() asks the function for the visible quads (camera at the end
// of the frame, tiles read then) into one shared scratch, within the frame's max_tiles budget.

#include "nnh/gfx.hpp"
#include "nnh/math.hpp"
#include "nnh/platform/video.hpp"

#include <cstdint>

namespace nnh::detail {

/// Batches per frame (both screens). A batch is a (map layer, tileset): 16 layers × 2 tilesets × 2 screens
/// fit.
inline constexpr int kMaxTileBatches = 64;
/// Quads one batch can hold: the most a screen shows of one layer (8×8 tiles on the top screen: 51 × 31).
inline constexpr int kTileScratchQuads = 1600;

struct TileExpandRequest {
    const void* source = nullptr; ///< what recorded the batch (a map's block)
    std::int32_t arg = 0;         ///< the recorder's own value (layer, tileset)
    Screen screen = Screen::Top;
    Vec2 camera{}; ///< the screen's camera at the end of the frame; zero when ignoring it
    Vec2 offset{}; ///< the draw's offset
    platform::TileQuad2D* out = nullptr;
    int capacity = 0; ///< quads `out` can take (scratch and budget)
};

struct TileExpandResult {
    int written = 0; ///< quads written to `out`
    int dropped = 0; ///< quads that did not fit
};

using TileExpandFn = TileExpandResult (*)(const TileExpandRequest& request);

struct TileBatchRecord {
    TileExpandFn expand = nullptr; ///< nullptr: forgotten (its source was destroyed)
    const void* source = nullptr;
    std::int32_t arg = 0;
    Vec2 offset{};
    int texture_slot = -1; ///< the tileset's texture handle, resolved at submit (stale = skipped)
    std::uint32_t texture_generation = 0;
    std::uint16_t tile_w = 0, tile_h = 0;
    Color color = colors::white; ///< alpha = opacity
    std::uint8_t layer = 0;      ///< 0..15
    bool ignore_camera = false;
};

class TileBatches {
  public:
    struct Quads {
        const platform::TileQuad2D* quads = nullptr;
        int count = 0;
    };

    /// Uses `records` (`capacity` long) and `scratch` (`scratch_capacity` quads); at most `max_tiles` quads
    /// per frame. Forgets everything.
    void reset(TileBatchRecord* records, int capacity, platform::TileQuad2D* scratch, int scratch_capacity,
               int max_tiles);

    /// Starts a new frame: no batches, no tiles counted.
    void clear();

    /// @return the batch's index, or -1 when the frame's batches are full.
    int add(const TileBatchRecord& record);

    /// The batch, or nullptr for a forgotten one or a bad index.
    const TileBatchRecord* get(int index) const;

    /// Forgets this frame's batches of `source` (it is being destroyed): they draw nothing.
    void forget(const void* source);

    /// Expands batch `index` into the scratch within the frame's remaining budget and counts its tiles and
    /// drops. The quads stay valid until the next expand(). Nothing for a forgotten batch.
    Quads expand(int index, Screen screen, Vec2 camera);

    int count() const { return count_; }
    int capacity() const { return capacity_; }
    int max_tiles() const { return max_tiles_; }
    int tiles() const { return tiles_; }     ///< quads expanded this frame
    int dropped() const { return dropped_; } ///< quads over the budget this frame

  private:
    TileBatchRecord* records_ = nullptr;
    platform::TileQuad2D* scratch_ = nullptr;
    int capacity_ = 0;
    int scratch_capacity_ = 0;
    int max_tiles_ = 0;
    int count_ = 0;
    int tiles_ = 0;
    int dropped_ = 0;
};

} // namespace nnh::detail
