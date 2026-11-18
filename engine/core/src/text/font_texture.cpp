// The built-in font's texture (font_texture.hpp).

#include "text/font_texture.hpp"

#include "gfx/gfx_state.hpp"
#include "log/engine_channels.hpp"
#include "memory/memory_internal.hpp"
#include "nnh/log.hpp"
#include "nnh/platform/app.hpp"
#include "nnh/platform/video.hpp"
#include "text/font_atlas.hpp"

namespace nnh {

NNH_MEM_TAG(font);

namespace {

int g_slot = -1;
std::uint32_t g_generation = 0;
std::size_t g_bytes = 0;

} // namespace

MemTag& detail::font_memory_tag() { return NNH_MEM_TAG_REF(font); }

bool detail::font_texture_create() {
    // The atlas is only needed until the platform has its copy.
    auto* atlas = static_cast<unsigned char*>(
        mem::alloc(MemRegion::Heap, NNH_MEM_TAG_REF(font), kAtlasSize * kAtlasSize * 4));
    if (atlas == nullptr) return false;
    build_font_atlas(atlas);
    platform::TextureInfo info;
    const bool created = platform::texture_create_rgba(atlas, kAtlasSize, kAtlasSize, info);
    mem::free(atlas);
    if (!created) {
        NNH_LOG_ERROR(gfx, "built-in font texture: %s", platform::last_error());
        return false;
    }
    g_slot = gfx_state().textures.add({info.handle, info.width, info.height, info.bytes}, g_generation);
    if (g_slot < 0) {
        platform::texture_destroy(info.handle);
        return false;
    }
    g_bytes = info.bytes;
    mem_account(MemRegion::Linear, NNH_MEM_TAG_REF(font), g_bytes, true);
    return true;
}

void detail::font_texture_release() {
    TextureData removed;
    if (g_slot >= 0 && gfx_state().textures.remove(g_slot, g_generation, removed)) {
        platform::texture_destroy(removed.platform);
        mem_account(MemRegion::Linear, NNH_MEM_TAG_REF(font), removed.bytes, false);
    }
    g_slot = -1;
    g_generation = 0;
    g_bytes = 0;
}

bool detail::font_texture_ready() { return g_slot >= 0; }
int detail::font_texture_slot() { return g_slot; }
std::uint32_t detail::font_texture_generation() { return g_generation; }
std::size_t detail::font_texture_bytes() { return g_bytes; }

} // namespace nnh
