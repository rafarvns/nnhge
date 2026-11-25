// 2D graphics lifecycle: capacities at init, end-of-frame submission per screen, deferred texture
// frees, cleanup at shutdown (docs/architecture/rendering.md).

#include "gfx/gfx3d_hook.hpp"
#include "gfx/gfx_internal.hpp"
#include "gfx/gfx_state.hpp"
#include "log/engine_channels.hpp"
#include "memory/memory_internal.hpp"
#include "nnh/assert.hpp"
#include "nnh/log.hpp"
#include "nnh/profile.hpp"
#include "text/font_texture.hpp"

namespace nnh {

NNH_MEM_TAG(gfx);

namespace {

detail::GfxState g_state;
Camera2D g_cameras[2];
detail::DrawCommand* g_commands = nullptr;
std::uint16_t* g_order = nullptr;
detail::TextureSlot* g_slots = nullptr;
detail::TileBatchRecord* g_tile_records = nullptr; // only when max_tiles > 0
platform::TileQuad2D* g_tile_scratch = nullptr;
std::uint32_t g_next_generation = 1; // texture handles from a previous init() stay stale
bool g_3d_on = false;                // the 3D hook's init succeeded (spec 026)

template <typename T>
T* allocate(int count) {
    return static_cast<T*>(mem::alloc(MemRegion::Heap, NNH_MEM_TAG_REF(gfx), sizeof(T) * static_cast<std::size_t>(count),
                                      alignof(T)));
}

void free_all() {
    mem::free(g_commands);
    mem::free(g_order);
    mem::free(g_state.pass);
    mem::free(g_slots);
    mem::free(g_state.pending);
    mem::free(g_state.debug_items);
    mem::free(g_tile_records);
    mem::free(g_tile_scratch);
    if (detail::g_gfx_particle_ops != nullptr) detail::g_gfx_particle_ops->shutdown();
    g_commands = nullptr;
    g_order = nullptr;
    g_slots = nullptr;
    g_tile_records = nullptr;
    g_tile_scratch = nullptr;
    g_state = detail::GfxState{};
    g_cameras[0] = g_cameras[1] = Camera2D{};
}

/// Expands tile batch `index` and sends it (in row chunks when it is larger than the scratch, zoom < 1).
/// @return 1 if its texture is gone (skipped), else 0.
int submit_tiles(Screen screen, int index, Camera2D camera) {
    detail::GfxState& g = g_state;
    const detail::TileBatchRecord* r = g.tiles.get(index);
    if (r == nullptr) return 0; // its map was destroyed during the frame
    const detail::TextureData* texture = g.textures.get(r->texture_slot, r->texture_generation);
    if (texture == nullptr) return 1;
    platform::TileBatch2D batch;
    batch.texture = texture->platform;
    batch.tile_w = r->tile_w;
    batch.tile_h = r->tile_h;
    batch.color = r->color;
    batch.layer = r->layer;
    int row = 0;
    do {
        const detail::TileBatches::Quads quads = g.tiles.expand(index, screen, camera, row);
        if (quads.count > 0) {
            batch.dst_w = quads.dst_w;
            batch.dst_h = quads.dst_h;
            batch.quads = quads.quads;
            batch.count = quads.count;
            platform::draw_tiles(screen, batch);
        }
        row = quads.next_row;
    } while (row >= 0);
    return 0;
}

/// The screen's 2D pass: the sorted items, cut by tile batches where they sit among them. With 3D on, the 3D module
/// submits screens that have 3D draws itself (the same 2D slices around its pass, spec 026 D43).
void submit_screen(Screen screen) {
    if (g_3d_on && detail::g_gfx_3d_ops->submit_screen(screen)) return;
    const detail::Pass2D pass = detail::gfx_build_pass(screen);
    const int stale_tiles = detail::gfx_submit_slice(screen, 0, pass.items, 0, pass.splits);
    detail::gfx_check_stale(pass, stale_tiles);
}

/// Allocates the tile batch records and quad scratch (nothing when max_tiles is 0). @return false if out of memory.
bool tiles_init(int max_tiles) {
    g_state.max_tiles = max_tiles < 0 ? 0 : (max_tiles > 65535 ? 65535 : max_tiles);
    if (g_state.max_tiles == 0) return true;
    const int scratch = g_state.max_tiles < detail::kTileScratchQuads ? g_state.max_tiles : detail::kTileScratchQuads;
    g_tile_records = allocate<detail::TileBatchRecord>(detail::kMaxTileBatches);
    g_tile_scratch = allocate<platform::TileQuad2D>(scratch);
    if (g_tile_records == nullptr || g_tile_scratch == nullptr) return false;
    g_state.tiles.reset(g_tile_records, detail::kMaxTileBatches, g_tile_scratch, scratch, g_state.max_tiles);
    return true;
}

} // namespace

constinit const detail::GfxParticleOps* detail::g_gfx_particle_ops = nullptr;

// The 2D part of submit_screen (spec 026 extracted it unchanged so the 3D pass can sit between two slices): items
// [item_from, item_to) of the built pass with the tile batches [split_from, split_to) where they sit among them.
int detail::gfx_submit_slice(Screen screen, int item_from, int item_to, int split_from, int split_to) {
    GfxState& g = g_state;
    const Camera2D camera = g_cameras[screen == Screen::Top ? 0 : 1];
    int stale_tiles = 0;
    int from = item_from;
    for (int i = split_from; i < split_to; ++i) {
        const TileSplit split = g.splits[i];
        if (split.at > from) platform::draw_2d(screen, g.pass + from, split.at - from);
        from = split.at;
        if (split.batch < kParticleBatchBase) {
            stale_tiles += submit_tiles(screen, split.batch, camera);
        } else if (g_gfx_particle_ops != nullptr) { // asserts itself on a stale texture
            g_gfx_particle_ops->submit(screen, split.batch - kParticleBatchBase, camera);
        }
    }
    if (item_to > from) platform::draw_2d(screen, g.pass + from, item_to - from);
    return stale_tiles;
}

detail::GfxState& detail::gfx_state() { return g_state; }
Camera2D* detail::gfx_cameras() { return g_cameras; }

bool detail::gfx_init(const GfxConfig& config) {
    free_all();
    int max_draws = config.max_draws < 16 ? 16 : config.max_draws;
    if (max_draws > 65535) max_draws = 65535;
    // One extra texture slot for the built-in font, so the game keeps its whole max_textures.
    const int max_textures = (config.max_textures < 4 ? 4 : config.max_textures) + 1;
    g_commands = allocate<DrawCommand>(max_draws);
    g_order = allocate<std::uint16_t>(max_draws);
    g_state.pass = allocate<platform::DrawItem2D>(max_draws);
    g_slots = allocate<TextureSlot>(max_textures);
    g_state.pending = allocate<PendingDestroy>(max_textures);
    g_state.debug_items = allocate<platform::DrawItem2D>(kDebugPassCapacity);
    if (g_commands == nullptr || g_order == nullptr || g_state.pass == nullptr || g_slots == nullptr ||
        g_state.pending == nullptr || g_state.debug_items == nullptr || !tiles_init(config.max_tiles) ||
        (detail::g_gfx_particle_ops != nullptr && !detail::g_gfx_particle_ops->init(config.max_particles))) {
        NNH_LOG_ERROR(gfx, "not enough memory for %d draws, %d textures and %d tiles (AppConfig::gfx)", max_draws,
                      max_textures - 1, config.max_tiles);
        free_all();
        return false;
    }
    g_state.draws.reset(g_commands, g_order, max_draws);
    g_state.textures.reset(g_slots, max_textures, g_next_generation);
    g_state.max_draws = max_draws;
    g_state.ready = true;
    if (!font_texture_create()) {
        gfx_shutdown();
        return false;
    }
    // 3D (spec 026, D43): only when the game links the 3D module and asks for a capacity.
    if (config.max_draws_3d > 0 && g_gfx_3d_ops != nullptr) {
        if (!g_gfx_3d_ops->init(config)) {
            gfx_shutdown();
            return false;
        }
        g_3d_on = true;
    }
    return true;
}

namespace {

/// Frees the `n` oldest pending textures and moves the rest to the front (D47: the GPU is done with them).
void free_pending(int n) {
    for (int i = 0; i < n; ++i) detail::destroy_texture_now(g_state.pending[i].slot, g_state.pending[i].generation);
    for (int i = n; i < g_state.pending_count; ++i) g_state.pending[i - n] = g_state.pending[i];
    g_state.pending_count -= n;
}

} // namespace

void detail::gfx_shutdown() {
    if (!g_state.ready) return;
    free_pending(g_state.pending_count); // core waited for the GPU (platform::finish_frames) before this
    g_state.retired_count = 0;
    if (g_3d_on) g_gfx_3d_ops->shutdown(); // meshes (and the textures they own) first
    g_3d_on = false;
    font_texture_release(); // counted under its own tag
    g_state.textures.for_each_active([](const TextureData& t) {
        platform::texture_destroy(t.platform);
        mem_account(MemRegion::Linear, NNH_MEM_TAG_REF(textures), t.bytes, false);
    });
    g_next_generation = g_state.textures.next_generation();
    free_all();
}

void detail::gfx_submit() {
    if (!g_state.ready) return;
#if defined(NNH_FLAVOR_DEVELOPMENT)
    NNH_PROFILE_SCOPE("gfx submit"); // Development only (the flavor benches measure in): spec 023's bench reads it
#endif
    submit_screen(Screen::Top);
    submit_screen(Screen::Bottom);
    g_state.draws_last_frame = g_state.draws.count();
    g_state.tiles_last_frame = g_state.tiles.tiles();
    const int dropped = g_state.draws.dropped();
    const int dropped_tiles = g_state.tiles.dropped();
    // Cleared before asserting: a crash here leaves the frame open, and shutdown() submits it again.
    g_state.draws.clear();
    g_state.tiles.clear();
    if (detail::g_gfx_particle_ops != nullptr) detail::g_gfx_particle_ops->end_frame();
    if (dropped > 0) {
        g_state.dropped_total += dropped;
        NNH_LOG_WARN(gfx, "%d draws dropped this frame (AppConfig::gfx.max_draws = %d)", dropped, g_state.max_draws);
    }
    if (dropped_tiles > 0) {
        g_state.dropped_tiles_total += dropped_tiles;
        NNH_LOG_WARN(gfx, "%d tile quads dropped this frame (AppConfig::gfx.max_tiles = %d)", dropped_tiles,
                     g_state.max_tiles);
        NNH_ASSERT(false, "more than %d tile quads in a frame: raise AppConfig::gfx.max_tiles", g_state.max_tiles);
    }
}

void detail::gfx_gpu_idle() {
    free_pending(g_state.retired_count);
    g_state.retired_count = 0;
    if (g_3d_on) g_gfx_3d_ops->gpu_idle();
}

void detail::gfx_frame_submitted() {
    g_state.retired_count = g_state.pending_count;
    if (g_3d_on) g_gfx_3d_ops->after_frame();
}

} // namespace nnh
