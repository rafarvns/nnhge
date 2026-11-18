// 2D graphics lifecycle: capacities at init, end-of-frame submission per screen, deferred texture
// frees, cleanup at shutdown (docs/architecture/rendering.md).

#include "gfx/gfx_internal.hpp"
#include "gfx/gfx_state.hpp"
#include "log/engine_channels.hpp"
#include "memory/memory_internal.hpp"
#include "nnh/assert.hpp"
#include "nnh/log.hpp"
#include "text/font_texture.hpp"

namespace nnh {

NNH_MEM_TAG(gfx);

namespace {

detail::GfxState g_state;
detail::DrawCommand* g_commands = nullptr;
std::uint16_t* g_order = nullptr;
detail::TextureSlot* g_slots = nullptr;
std::uint32_t g_next_generation = 1; // texture handles from a previous init() stay stale

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
    g_commands = nullptr;
    g_order = nullptr;
    g_slots = nullptr;
    g_state = detail::GfxState{};
}

void submit_screen(Screen screen) {
    detail::GfxState& g = g_state;
    const auto result = g.draws.build(screen, g.cameras[screen == Screen::Top ? 0 : 1].position, g.textures, g.pass,
                                      g.max_draws);
    NNH_ASSERT(result.stale == 0, "%d sprites drawn with a texture destroyed earlier", result.stale);
    if (result.items > 0) platform::draw_2d(screen, g.pass, result.items);
}

} // namespace

detail::GfxState& detail::gfx_state() { return g_state; }

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
        g_state.pending == nullptr || g_state.debug_items == nullptr) {
        NNH_LOG_ERROR(gfx, "not enough memory for %d draws and %d textures (AppConfig::gfx)", max_draws,
                      max_textures - 1);
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
    return true;
}

void detail::gfx_shutdown() {
    if (!g_state.ready) return;
    gfx_after_frame();
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
    submit_screen(Screen::Top);
    submit_screen(Screen::Bottom);
    g_state.draws_last_frame = g_state.draws.count();
    if (const int dropped = g_state.draws.dropped(); dropped > 0) {
        g_state.dropped_total += dropped;
        NNH_LOG_WARN(gfx, "%d draws dropped this frame (AppConfig::gfx.max_draws = %d)", dropped, g_state.max_draws);
    }
    g_state.draws.clear();
}

void detail::gfx_after_frame() {
    for (int i = 0; i < g_state.pending_count; ++i) {
        destroy_texture_now(g_state.pending[i].slot, g_state.pending[i].generation);
    }
    g_state.pending_count = 0;
}

} // namespace nnh
