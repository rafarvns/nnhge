#pragma once
// State shared by the 2D graphics files (textures.cpp, draw2d.cpp, gfx_frame.cpp). Internal.

#include <cstdint>

#include "gfx/draw_list.hpp"
#include "gfx/texture_table.hpp"
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
};

namespace detail {

struct PendingDestroy {
    int slot = -1;
    std::uint32_t generation = 0;
};

struct GfxState {
    bool ready = false;
    TextureTable textures;
    DrawList draws;
    platform::DrawItem2D* pass = nullptr; // max_draws items, reused for every pass
    PendingDestroy* pending = nullptr;    // textures released during the frame (max_textures)
    int pending_count = 0;
    int max_draws = 0;
    Camera2D cameras[2];
    int draws_last_frame = 0;
    int dropped_total = 0;
};

GfxState& gfx_state();

/// True inside a frame with the graphics module ready (draw calls are recorded).
bool gfx_drawing();

/// Records a 2D draw; on overflow asserts (Debug/Development) or drops it (Release).
void gfx_record(const DrawCommand& command);

/// Clamps a layer to 0..15.
std::uint8_t gfx_clamp_layer(int layer);

/// Frees a texture's platform resource and memory accounting now.
void destroy_texture_now(int slot, std::uint32_t generation);

} // namespace detail
} // namespace nnh
