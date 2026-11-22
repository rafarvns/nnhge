#pragma once
// State shared by the 2D graphics files (textures.cpp, draw2d.cpp, gfx_frame.cpp). Internal.

#include <cstdint>

#include "gfx/draw_list.hpp"
#include "gfx/texture_table.hpp"
#include "gfx/tile_batches.hpp"
#include "nnh/gfx.hpp"
#include "nnh/memory.hpp"
#include "nnh/platform/video.hpp"
#include "nnh/texture.hpp"

namespace nnh {

NNH_MEM_TAG_EXTERN(textures);

/// Reaches into nnh::Texture for the engine (friend of Texture).
struct TextureAccess {
    static int slot(const Texture& t) { return t.slot_; }
    static std::uint32_t generation(const Texture& t) { return t.generation_; }
    static Texture make(int slot, std::uint32_t generation, int width, int height) {
        Texture t;
        t.slot_ = slot;
        t.generation_ = generation;
        t.width_ = width;
        t.height_ = height;
        return t;
    }
    /// Empties `t` without freeing its texture: its owner is now someone else (a loaded map).
    static void release(Texture& t) {
        t.slot_ = -1;
        t.generation_ = 0;
        t.width_ = t.height_ = 0;
    }
};

namespace detail {

/// Items of the debug pass per screen (debug text glyphs + overlay rectangles), apart from max_draws.
inline constexpr int kDebugPassCapacity = 4096;

struct PendingDestroy {
    int slot = -1;
    std::uint32_t generation = 0;
};

struct GfxState {
    bool ready = false;
    TextureTable textures;
    DrawList draws;
    platform::DrawItem2D* pass = nullptr; // max_draws items, reused for every pass
    platform::DrawItem2D* debug_items = nullptr; // kDebugPassCapacity items for the debug pass
    PendingDestroy* pending = nullptr;    // textures released during the frame (max_textures)
    int pending_count = 0;
    int max_draws = 0;
    int draws_last_frame = 0;
    int dropped_total = 0;
    // Tile batches (spec 012): storage only when max_tiles > 0.
    TileBatches tiles;
    TileSplit splits[kMaxTileBatches]; // one screen's split points, reused per pass
    int max_tiles = 0;
    int tiles_last_frame = 0;
    int dropped_tiles_total = 0;
    bool warned_tiles_off = false; // Release: "max_tiles is 0" logged once
};

GfxState& gfx_state();

/// The screens' cameras (top, bottom), reset to {(0, 0), zoom 1} at init and shutdown. Kept apart from GfxState so
/// that the zoom's non-zero default doesn't move the whole state from .bss to .data (spec 018 stage 4).
Camera2D* gfx_cameras();

/// True inside a frame with the graphics module ready (draw calls are recorded).
bool gfx_drawing();

/// Records a 2D draw; on overflow asserts (Debug/Development) or drops it (Release).
void gfx_record(const DrawCommand& command);

/// True when AppConfig::gfx.max_tiles > 0; otherwise asserts with the fix (Release: logs once) and returns
/// false.
bool gfx_tiles_enabled();

/// Records a tile batch on `screen` at draw `layer` (gfx::draw_tilemap): it keeps its place among the 2D
/// draws and is expanded at submit. Asserts (and records nothing in Release) when max_tiles is 0 or the frame
/// already has kMaxTileBatches batches. Only inside a frame.
void gfx_record_tiles(Screen screen, int layer, const TileBatchRecord& record);

/// Forgets this frame's tile batches of `source` (a tilemap being destroyed).
void gfx_forget_tiles(const void* source);

/// Clamps a layer to 0..15.
std::uint8_t gfx_clamp_layer(int layer);

/// Frees a texture's platform resource and memory accounting now.
void destroy_texture_now(int slot, std::uint32_t generation);

} // namespace detail
} // namespace nnh
