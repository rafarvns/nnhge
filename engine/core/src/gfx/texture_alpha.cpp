// Glyph pages as gfx textures (texture_alpha.hpp).

#include "gfx/texture_alpha.hpp"

#include "gfx/gfx_state.hpp"
#include "memory/memory_internal.hpp"
#include "nnh/platform/video.hpp"
#include "nnh/texture.hpp"

namespace nnh {

int detail::texture_from_page(const std::uint8_t* pixels, int width, int height, bool rgba, MemTag& tag,
                              std::uint32_t& generation) {
    GfxState& g = gfx_state();
    platform::TextureInfo info;
    const bool made = rgba ? platform::texture_create_rgba(pixels, width, height, info)
                           : platform::texture_create_alpha(pixels, width, height, info);
    if (!made) return -1;
    const int slot = g.textures.add({info.handle, info.width, info.height, info.bytes}, generation);
    if (slot < 0) {
        platform::texture_destroy(info.handle);
        return -2;
    }
    mem_account(MemRegion::Linear, tag, info.bytes, true);
    return slot;
}

void detail::texture_release_page(int slot, std::uint32_t generation, MemTag& tag) {
    const TextureData* data = gfx_state().textures.get(slot, generation);
    if (data == nullptr) return;
    mem_account(MemRegion::Linear, tag, data->bytes, false);
    mem_account(MemRegion::Linear, NNH_MEM_TAG_REF(textures), data->bytes, true);
    Texture t = TextureAccess::make(slot, generation, data->width, data->height);
    t.reset(); // now, or deferred until the GPU is done (D47): textures.cpp counts it out of `textures`
}

} // namespace nnh
