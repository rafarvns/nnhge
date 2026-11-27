// The texture kind of hot reload (spec 031 FR-7, FR-16, plan question 6): each texture slot loaded from an asset path
// gets that path's hash in a Debug/Development side table; a reloaded `*.png` is decoded for every slot loaded from it
// at prepare (a new platform texture, its size checked, Linear memory accounted), and at commit the slot's data is
// swapped behind the game's handles — `Texture::width()/height()` read the slot in Debug/Development builds — and the old
// platform texture is retired until the GPU is idle (D47). Tilemap tilesets, UI images and mesh textures keep their slot
// and follow. Tests: tests/gfx/test_texture_reload.cpp.

#include <cstring>

#include "core/shutdown_hooks.hpp"
#include "gfx/gfx_internal.hpp"
#include "gfx/gfx_state.hpp"
#include "gfx/texture_reload.hpp"
#include "gfx/texture_rules.hpp"
#include "memory/memory_internal.hpp"
#include "nnh/assets.hpp"
#include "nnh/memory.hpp"
#include "nnh/platform/app.hpp"
#include "nnh/texture.hpp"
#include "reload/reload_kind.hpp"
#include "reload/reload_memory.hpp"
#include "reload/reload_overlay.hpp"

namespace nnh {

NNH_MEM_TAG_EXTERN(textures);

namespace detail {

void (*g_texture_retire_hook)() = nullptr;

namespace {

using reload::Prepared;
using reload::ReloadItem;
using remote::ReloadReason;
using remote::ReloadStage;

struct SlotTag {
    std::uint32_t hash = 0;       // 0: not loaded from an asset path
    std::uint32_t generation = 0;
};

SlotTag* g_tags = nullptr;
int g_tag_count = 0;

/// Platform textures replaced by a reload, destroyed at the next GPU idle.
constexpr int kMaxRetired = 64;
std::uintptr_t g_retired[kMaxRetired];
int g_retired_count = 0;

struct NewTexture {
    int slot = 0;
    std::uint32_t generation = 0;
    platform::TextureInfo info;
};
struct PreparedTextures {
    int count = 0;
    NewTexture items[1]; // `count` of them
};

void retire_now() {
    for (int i = 0; i < g_retired_count; ++i) platform::texture_destroy(g_retired[i]);
    g_retired_count = 0;
    g_texture_retire_hook = nullptr;
}

void retire(std::uintptr_t handle) {
    if (g_retired_count == kMaxRetired) retire_now(); // the GPU is idle at the apply point: safe, rare
    g_retired[g_retired_count++] = handle;
    g_texture_retire_hook = retire_now;
}

bool claims(const ReloadItem& item) {
    const std::size_t n = std::strlen(item.path);
    return n > 4 && std::strcmp(item.path + n - 4, ".png") == 0;
}

int slots_from(std::uint32_t hash) {
    int n = 0;
    const GfxState& g = gfx_state();
    for (int i = 0; i < g_tag_count; ++i) {
        if (g_tags[i].hash == hash && g.textures.get(i, g_tags[i].generation) != nullptr) ++n;
    }
    return n;
}

void destroy_prepared(PreparedTextures* p, int upto) {
    for (int i = 0; i < upto; ++i) platform::texture_destroy(p->items[i].info.handle);
    mem::free(p);
}

Prepared prepare(const ReloadItem& item) {
    const int count = item.op == remote::ReloadOp::Put ? slots_from(item.hash) : 0;
    if (count == 0) return reload::prepared_ok(); // nothing loaded from it (a removed path keeps its content)
    auto file = assets::load(item.path); // the batch's new bytes
    if (!file) return reload::prepared_refused(ReloadStage::Memory, ReloadReason::NoMemory, item.size);
    const std::size_t bytes = sizeof(PreparedTextures) + sizeof(NewTexture) * static_cast<std::size_t>(count - 1);
    auto* p = static_cast<PreparedTextures*>(mem::alloc(MemRegion::Heap, reload::reload_memory_tag(), bytes));
    if (p == nullptr) return reload::prepared_refused(ReloadStage::Memory, ReloadReason::NoMemory, bytes);
    p->count = 0;
    const GfxState& g = gfx_state();
    for (int i = 0; i < g_tag_count; ++i) {
        if (g_tags[i].hash != item.hash || g.textures.get(i, g_tags[i].generation) == nullptr) continue;
        NewTexture& t = p->items[p->count];
        if (!platform::texture_create(file.value().data(), file.value().size(), t.info)) {
            destroy_prepared(p, p->count); // no GPU memory left, or not an image
            return reload::prepared_refused(ReloadStage::Memory, ReloadReason::NoMemory, item.size);
        }
        if (!valid_texture_size(t.info.width, t.info.height)) {
            platform::texture_destroy(t.info.handle);
            destroy_prepared(p, p->count);
            return reload::prepared_refused(ReloadStage::Validate, ReloadReason::Corrupted);
        }
        t.slot = i;
        t.generation = g_tags[i].generation;
        ++p->count;
    }
    return reload::prepared_ok(p);
}

void commit(const ReloadItem&, void* state) {
    auto* p = static_cast<PreparedTextures*>(state);
    if (p == nullptr) return;
    GfxState& g = gfx_state();
    for (int i = 0; i < p->count; ++i) {
        const NewTexture& t = p->items[i];
        TextureData* data = g.textures.mutable_get(t.slot, t.generation);
        if (data == nullptr) {
            platform::texture_destroy(t.info.handle); // released between prepare and commit: can't happen, but safe
            continue;
        }
        retire(data->platform);
        mem_account(MemRegion::Linear, NNH_MEM_TAG_REF(textures), data->bytes, false);
        mem_account(MemRegion::Linear, NNH_MEM_TAG_REF(textures), t.info.bytes, true);
        *data = TextureData{t.info.handle, t.info.width, t.info.height, t.info.bytes};
    }
    mem::free(p);
}

void abandon(const ReloadItem&, void* state) {
    auto* p = static_cast<PreparedTextures*>(state);
    if (p != nullptr) destroy_prepared(p, p->count);
}

reload::ReloadKind g_kind{"texture", claims, prepare, commit, abandon};

void free_tags() {
    mem::free(g_tags);
    g_tags = nullptr;
    g_tag_count = 0;
}

void texture_reload_shutdown() {
    retire_now();
    free_tags();
}

} // namespace

void texture_reload_loaded(int slot, std::uint32_t generation, const char* asset_path) {
    if (g_tags == nullptr) {
        const int capacity = gfx_state().textures.capacity();
        g_tags = static_cast<SlotTag*>(mem::alloc(MemRegion::Heap, reload::reload_memory_tag(),
                                                  sizeof(SlotTag) * static_cast<std::size_t>(capacity)));
        if (g_tags == nullptr) return; // this texture won't reload (logged nowhere: development memory only)
        g_tag_count = capacity;
        for (int i = 0; i < capacity; ++i) g_tags[i] = SlotTag{};
    }
    reload::register_kind(g_kind);
    require_shutdown_hook(texture_reload_shutdown);
    if (slot >= 0 && slot < g_tag_count) g_tags[slot] = SlotTag{reload::path_hash(asset_path), generation};
}

int texture_width_dev(const Texture& t) {
    const TextureData* d = t.valid() ? gfx_state().textures.get(TextureAccess::slot(t), TextureAccess::generation(t))
                                     : nullptr;
    return d != nullptr ? d->width : t.cached_width();
}

int texture_height_dev(const Texture& t) {
    const TextureData* d = t.valid() ? gfx_state().textures.get(TextureAccess::slot(t), TextureAccess::generation(t))
                                     : nullptr;
    return d != nullptr ? d->height : t.cached_height();
}

} // namespace detail
} // namespace nnh
