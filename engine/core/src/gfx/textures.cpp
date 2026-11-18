// Textures (nnh/texture.hpp): loading embedded images through the platform, releasing them safely.

#include <cstdio>
#include <cstring>

#include "core/engine_state.hpp"
#include "gfx/gfx_state.hpp"
#include "nnh/detail/image_registry.hpp"
#include "gfx/texture_rules.hpp"
#include "log/engine_channels.hpp"
#include "memory/memory_internal.hpp"
#include "nnh/log.hpp"
#include "nnh/platform/app.hpp"
#include "nnh/texture.hpp"

namespace nnh {

NNH_MEM_TAG(textures);

namespace {

const detail::EmbeddedImage* find_image(const char* name) {
    int count = 0;
    const detail::EmbeddedImage* images = detail::embedded_images(count);
    for (int i = 0; i < count; ++i) {
        if (std::strcmp(images[i].name, name) == 0) return &images[i];
    }
    return nullptr;
}

Error refuse(ErrorCode code, const char* name, const char* what) {
    NNH_LOG_WARN(gfx, "load_texture(\"%s\"): %s", name, what);
    return Error::make(code, "image \"%s\": %s", name, what);
}

} // namespace

void detail::destroy_texture_now(int slot, std::uint32_t generation) {
    TextureData removed;
    if (!gfx_state().textures.remove(slot, generation, removed)) return;
    platform::texture_destroy(removed.platform);
    mem_account(MemRegion::Linear, NNH_MEM_TAG_REF(textures), removed.bytes, false);
}

Texture::Texture(Texture&& other) noexcept
    : slot_(other.slot_), generation_(other.generation_), width_(other.width_), height_(other.height_) {
    other.slot_ = -1;
    other.generation_ = 0;
    other.width_ = other.height_ = 0;
}

Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        reset();
        slot_ = other.slot_;
        generation_ = other.generation_;
        width_ = other.width_;
        height_ = other.height_;
        other.slot_ = -1;
        other.generation_ = 0;
        other.width_ = other.height_ = 0;
    }
    return *this;
}

bool Texture::valid() const {
    const detail::GfxState& g = detail::gfx_state();
    return slot_ >= 0 && g.ready && g.textures.get(slot_, generation_) != nullptr;
}

void Texture::reset() {
    if (valid()) {
        detail::GfxState& g = detail::gfx_state();
        if (detail::frame_open()) {
            // Draws recorded this frame may still use it: free it after the frame (gfx_after_frame).
            g.pending[g.pending_count++] = detail::PendingDestroy{slot_, generation_};
        } else {
            detail::destroy_texture_now(slot_, generation_);
        }
    }
    slot_ = -1;
    generation_ = 0;
    width_ = height_ = 0;
}

Result<Texture> load_texture(const char* name) {
    detail::GfxState& g = detail::gfx_state();
    if (name == nullptr) name = "";
    if (!g.ready) return Error::make(ErrorCode::Unsupported, "load_texture(\"%s\") before nnh::init()", name);
    const detail::EmbeddedImage* image = find_image(name);
    if (image == nullptr) return refuse(ErrorCode::NotFound, name, "not in this game's IMAGES");

    platform::TextureInfo info;
    if (!platform::texture_create(image->data, image->size, info)) {
        NNH_LOG_WARN(gfx, "load_texture(\"%s\"): %s", name, platform::last_error());
        return Error::make(ErrorCode::Corrupted, "image \"%s\" could not be loaded", name);
    }
    if (!detail::valid_texture_size(info.width, info.height)) {
        platform::texture_destroy(info.handle);
        char what[64];
        std::snprintf(what, sizeof what, "%dx%d, sides must be 2^n from 8 to 1024", info.width, info.height);
        return refuse(ErrorCode::InvalidArgument, name, what);
    }
    std::uint32_t generation = 0;
    const int slot = g.textures.add({info.handle, info.width, info.height, info.bytes}, generation);
    if (slot < 0) {
        platform::texture_destroy(info.handle);
        return refuse(ErrorCode::LimitReached, name, "too many textures (AppConfig::gfx.max_textures)");
    }
    detail::mem_account(MemRegion::Linear, NNH_MEM_TAG_REF(textures), info.bytes, true);
    return TextureAccess::make(slot, generation, info.width, info.height);
}

} // namespace nnh
