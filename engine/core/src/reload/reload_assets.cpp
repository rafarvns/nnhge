// Asset reads through hot reload (see reload_assets.hpp).

#include "reload/reload_assets.hpp"

#include <cstring>

#include "assets/asset_index.hpp"
#include "nnh/memory.hpp"
#include "nnh/platform/reload.hpp"
#include "reload/reload_files.hpp"
#include "reload/reload_memory.hpp"
#include "reload/reload_overlay.hpp"

namespace nnh::detail {


namespace reload {
namespace {

const BatchFile* g_staging = nullptr;
int g_staging_count = 0;
const AssetIndex* g_current = nullptr;
OverlayTable g_overlay;
OverlayEntry* g_slots = nullptr;

const AssetIndex* index_now() { return g_current != nullptr ? g_current : g_reload_original_index; }

} // namespace

void set_staging(const BatchFile* files, int count) {
    g_staging = files;
    g_staging_count = count;
}

void set_current_index(const AssetIndex* index) { g_current = index; }
const AssetIndex* current_index() { return index_now(); }
OverlayTable* overlay() { return g_slots != nullptr ? &g_overlay : nullptr; }

bool activate() {
    if (g_slots == nullptr) {
        g_slots = static_cast<OverlayEntry*>(
            mem::alloc(MemRegion::Heap, reload_memory_tag(), sizeof(OverlayEntry) * kOverlaySlots));
        if (g_slots == nullptr) return false;
        g_overlay.attach(g_slots);
    }
    g_reload_assets_active = true;
    return true;
}

void deactivate() {
    g_reload_assets_active = false;
    g_staging = nullptr;
    g_staging_count = 0;
    g_current = nullptr;
    mem::free(g_slots);
    g_slots = nullptr;
    g_overlay = OverlayTable{};
}

} // namespace reload

bool reload_asset_lookup(const char* path, AssetRedirect& out) {
    out = AssetRedirect{};
    for (int i = 0; i < reload::g_staging_count; ++i) {
        const reload::BatchFile& f = reload::g_staging[i];
        if (std::strcmp(f.path, path) != 0) continue;
        if (f.op == remote::ReloadOp::Remove) return false;
        out = {f.path, f.file, f.size};
        return true;
    }
    const AssetIndex* index = reload::index_now();
    const char* stored = index != nullptr ? index->find(path) : nullptr;
    if (stored == nullptr) return false;
    out.stored = stored;
    if (const reload::OverlayEntry* e = reload::g_overlay.find(stored, reload::path_hash(stored))) {
        out.file = e->file;
        out.size = e->size;
    }
    return true;
}

const char* reload_asset_case_hint(const char* path) {
    const AssetIndex* index = reload::index_now();
    return index != nullptr ? index->find_ignoring_case(path) : nullptr;
}

bool reload_read_file(const AssetRedirect& file, void* buffer, std::size_t& read) {
    read = 0;
    char name[32];
    reload::reload_file_name(file.file, name);
    platform::AssetStream stream = 0;
    if (!platform::reload_stream_open(name, stream)) return false;
    const bool ok = file.size == 0 || platform::asset_read_at(stream, 0, buffer, file.size, read);
    platform::asset_close(stream);
    return ok;
}

bool reload_asset_open(const char* path, platform::AssetStream& out, bool shared) {
    AssetRedirect r;
    if (g_reload_assets_active) {
        if (!reload_asset_lookup(path, r)) return false;
        if (r.file != 0) {
            char name[32];
            reload::reload_file_name(r.file, name);
            return platform::reload_stream_open(name, out);
        }
    }
    return shared ? platform::asset_open_shared(path, out) : platform::asset_open(path, out);
}

const ReloadAssetOps kReloadAssetOps{reload_asset_lookup, reload_asset_case_hint, reload_read_file, reload_asset_open};

} // namespace nnh::detail
