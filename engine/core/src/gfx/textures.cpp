// Textures (nnh/texture.hpp): loading embedded images through the platform, releasing them safely.

#include <cstdio>
#include <cstring>
#include <utility>

#include "core/engine_state.hpp"
#include "assets/asset_path.hpp"
#include "assets/assets_internal.hpp"
#include "gfx/gfx_state.hpp"
#include "gfx/texture_reload.hpp"
#include "nnh/build.hpp"
#include "nnh/assets.hpp"
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
        if (detail::gpu_may_use_resources()) {
            // Draws recorded this frame, or a frame the GPU still draws, may use it: freed once the GPU is done with
            // every frame submitted until now (gfx_frame_submitted, then gfx_gpu_idle; D47).
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
    // The game's assets first ("characters/hero" -> assets/characters/hero.png), then IMAGES by name.
    char asset_path[detail::kMaxAssetPath + 1];
    AssetData asset_bytes;
    const void* data = nullptr;
    std::size_t data_size = 0;
    if (std::snprintf(asset_path, sizeof asset_path, "%s.png", name) < static_cast<int>(sizeof asset_path) &&
        detail::asset_lookup(asset_path) != nullptr) {
        auto loaded = assets::load(asset_path);
        if (!loaded) return loaded.error();
        asset_bytes = std::move(loaded.value());
        data = asset_bytes.data();
        data_size = asset_bytes.size();
    } else if (const detail::EmbeddedImage* image = find_image(name)) {
        data = image->data;
        data_size = image->size;
    } else {
        return refuse(ErrorCode::NotFound, name, "not in the game's assets or IMAGES");
    }

    platform::TextureInfo info;
    if (!platform::texture_create(data, data_size, info)) {
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
    if constexpr (build::kDevToolsEnabled) { // spec 031: a texture made from an asset reloads with it
        if (asset_bytes.valid()) detail::texture_reload_loaded(slot, generation, asset_path);
    }
    return TextureAccess::make(slot, generation, info.width, info.height);
}

} // namespace nnh
