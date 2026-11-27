// The sprite libraries a hot reload can reach (spec 031 FR-7, FR-8; Debug/Development): up to 64 libraries loaded from
// an asset path, each with its identity block (the game's), its current block and the previous one. A small table (tag
// `animation`) allocated by the first load_sprite_library; the library queries (sprite_view), the animators' rebind and
// the destructor read it. Kept apart from the reload kind (sprite_reload.cpp) so that animators link no reload code.
// Tests: tests/animation/test_sprite_reload.cpp.

#include <cstdio>
#include <new>

#include "animation/sprite_library_internal.hpp"
#include "core/shutdown_hooks.hpp"

namespace nnh::detail {

int g_sprite_reloads = 0;

namespace {

constexpr int kEntries = 64;
SpriteReloadEntry* g_entries = nullptr;

void shutdown_hook() {
    mem::free(g_entries);
    g_entries = nullptr;
    g_sprite_reloads = 0;
}

std::uint32_t name_hash(const char* path) { // FNV-1a, as reload::path_hash (kept here: no reload code linked)
    std::uint32_t h = 2166136261u;
    for (; *path != '\0'; ++path) h = (h ^ static_cast<unsigned char>(*path)) * 16777619u;
    return h;
}

} // namespace

SpriteReloadEntry* sprite_reload_entry(const SpriteBlock* identity) {
    if (g_entries == nullptr || identity == nullptr) return nullptr;
    for (int i = 0; i < kEntries; ++i) {
        if (g_entries[i].identity == identity) return &g_entries[i];
    }
    return nullptr;
}

SpriteReloadEntry* sprite_reload_entry_of_serial(std::uint16_t serial) {
    if (g_entries == nullptr || serial == 0) return nullptr;
    for (int i = 0; i < kEntries; ++i) {
        if (g_entries[i].identity != nullptr && g_entries[i].serial == serial) return &g_entries[i];
    }
    return nullptr;
}

SpriteReloadEntry* sprite_reload_entries(int& count) {
    count = g_entries != nullptr ? kEntries : 0;
    return g_entries;
}

bool sprite_reload_track(SpriteBlock* identity, const char* asset_path, const char* name,
                         const LoadSpriteLibraryOptions& o) {
    if (g_entries == nullptr) {
        g_entries = static_cast<SpriteReloadEntry*>(
            mem::alloc(MemRegion::Heap, animation_memory_tag(), sizeof(SpriteReloadEntry) * kEntries));
        if (g_entries == nullptr) return false;
        for (int i = 0; i < kEntries; ++i) new (&g_entries[i]) SpriteReloadEntry{};
        (void)add_shutdown_hook(shutdown_hook);
    }
    for (int i = 0; i < kEntries; ++i) {
        SpriteReloadEntry& e = g_entries[i];
        if (e.identity != nullptr) continue;
        e = SpriteReloadEntry{};
        e.identity = identity;
        e.current = identity;
        e.hash = name_hash(asset_path);
        e.serial = identity->header.serial;
        const int values[6] = {o.extra_frames, o.extra_clips, o.extra_clip_frames, o.extra_markers, o.extra_states,
                               o.extra_transitions};
        for (int s = 0; s < 6; ++s) e.extras[s] = static_cast<std::uint16_t>(values[s]);
        std::snprintf(e.name, sizeof e.name, "%s", name);
        return true;
    }
    return false; // 64 libraries from assets: the 65th doesn't reload
}

void sprite_reload_forget(SpriteBlock* identity) {
    SpriteReloadEntry* e = sprite_reload_entry(identity);
    if (e == nullptr) return;
    if (e->current != identity) mem::free(e->current);
    mem::free(e->previous);
    *e = SpriteReloadEntry{};
}

} // namespace nnh::detail
