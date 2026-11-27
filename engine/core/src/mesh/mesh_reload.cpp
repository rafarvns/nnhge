// The mesh kind of hot reload (spec 031 FR-7): each mesh slot loaded from an asset path gets the path's hash in a
// Debug/Development side table; a reloaded `*.nnhmesh` is validated, uploaded and given its textures for every slot loaded
// from it at prepare (a new record), and at commit the slot's record is swapped behind the game's Mesh handles (more
// vertices, submeshes or textures are fine). The old record is freed at once: the apply point comes after this frame's
// GPU wait and before anything is recorded, and a paused frame never applies a batch (D47's condition holds).
// Tests: tests/mesh/test_mesh_reload.cpp.

#include <cstring>

#include "core/shutdown_hooks.hpp"
#include "mesh/mesh_internal.hpp"
#include "nnh/memory.hpp"
#include "reload/reload_kind.hpp"
#include "reload/reload_memory.hpp"
#include "reload/reload_overlay.hpp"

namespace nnh::detail {
namespace {

using reload::Prepared;
using reload::ReloadItem;
using remote::ReloadReason;
using remote::ReloadStage;

struct SlotTag {
    std::uint32_t hash = 0;
    std::uint32_t generation = 0;
};
SlotTag* g_tags = nullptr; // kMeshSlots entries

struct NewMesh {
    int slot = 0;
    std::uint32_t generation = 0;
    MeshRecord* record = nullptr;
};
struct PreparedMeshes {
    int count = 0;
    NewMesh items[1];
};

bool claims(const ReloadItem& item) {
    const std::size_t n = std::strlen(item.path);
    return n > 8 && std::strcmp(item.path + n - 8, ".nnhmesh") == 0;
}

bool live(int i) { return g_tags[i].hash != 0 && mesh_get(i, g_tags[i].generation) != nullptr; }

void destroy_prepared(PreparedMeshes* p) {
    for (int i = 0; i < p->count; ++i) mesh_record_destroy(p->items[i].record);
    mem::free(p);
}

Prepared prepare(const ReloadItem& item) {
    if (item.op != remote::ReloadOp::Put || g_tags == nullptr) return reload::prepared_ok();
    int count = 0;
    for (int i = 0; i < kMeshSlots; ++i) count += g_tags[i].hash == item.hash && live(i) ? 1 : 0;
    if (count == 0) return reload::prepared_ok();
    auto file = assets::load(item.path); // the batch's new bytes
    if (!file) return reload::prepared_refused(ReloadStage::Memory, ReloadReason::NoMemory, item.size);
    const std::size_t bytes = sizeof(PreparedMeshes) + sizeof(NewMesh) * static_cast<std::size_t>(count - 1);
    auto* p = static_cast<PreparedMeshes*>(mem::alloc(MemRegion::Heap, reload::reload_memory_tag(), bytes));
    if (p == nullptr) return reload::prepared_refused(ReloadStage::Memory, ReloadReason::NoMemory, bytes);
    p->count = 0;
    for (int i = 0; i < kMeshSlots; ++i) {
        if (g_tags[i].hash != item.hash || !live(i)) continue;
        Error error;
        MeshRecord* record = mesh_record_from_file(file.value(), item.path, error);
        if (record == nullptr) {
            destroy_prepared(p);
            const bool memory = error.code == ErrorCode::OutOfMemory || error.code == ErrorCode::LimitReached;
            return reload::prepared_refused(memory ? ReloadStage::Memory : ReloadStage::Validate,
                                            memory ? ReloadReason::NoMemory : ReloadReason::Corrupted, item.size);
        }
        p->items[p->count++] = NewMesh{i, g_tags[i].generation, record};
    }
    return reload::prepared_ok(p);
}

void commit(const ReloadItem&, void* state) {
    auto* p = static_cast<PreparedMeshes*>(state);
    if (p == nullptr) return;
    for (int i = 0; i < p->count; ++i) {
        MeshRecord* old = mesh_replace(p->items[i].slot, p->items[i].generation, p->items[i].record);
        if (old != nullptr) mesh_record_destroy(old);
        else mesh_record_destroy(p->items[i].record); // released meanwhile: can't happen, but nothing leaks
    }
    mem::free(p);
}

void abandon(const ReloadItem&, void* state) {
    if (auto* p = static_cast<PreparedMeshes*>(state)) destroy_prepared(p);
}

reload::ReloadKind g_kind{"mesh", claims, prepare, commit, abandon};

void shutdown_hook() {
    mem::free(g_tags);
    g_tags = nullptr;
}

} // namespace

void mesh_reload_loaded(int slot, std::uint32_t generation, const char* asset_path) {
    if (g_tags == nullptr) {
        g_tags = static_cast<SlotTag*>(
            mem::alloc(MemRegion::Heap, reload::reload_memory_tag(), sizeof(SlotTag) * kMeshSlots));
        if (g_tags == nullptr) return;
        for (int i = 0; i < kMeshSlots; ++i) g_tags[i] = SlotTag{};
        require_shutdown_hook(shutdown_hook);
    }
    reload::register_kind(g_kind);
    if (slot >= 0 && slot < kMeshSlots) g_tags[slot] = SlotTag{reload::path_hash(asset_path), generation};
}

} // namespace nnh::detail
