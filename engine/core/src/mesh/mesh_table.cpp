// The mesh table (mesh_internal.hpp): 256 slots with generations; a released mesh is kept until the GPU is done with
// every frame that may draw it (D47).

#include "core/engine_state.hpp"
#include "memory/memory_internal.hpp"
#include "mesh/mesh_internal.hpp"

namespace nnh {

NNH_MEM_TAG(meshes);

namespace detail {
namespace {

constexpr std::uint8_t kLive = 0;
constexpr std::uint8_t kReleased = 1;
constexpr std::uint8_t kRetired = 2;

struct Slot {
    MeshRecord* record = nullptr;
    std::uint32_t generation = 0;
    std::uint8_t stage = 0; ///< kLive, kReleased (kept: in use), kRetired (submitted since: freed at the next wait)
};

Slot* g_slots = nullptr;
std::uint32_t g_next_generation = 1; // handles from an earlier init() stay stale

} // namespace

bool mesh_table_init() {
    if (g_slots != nullptr) return true;
    g_slots = static_cast<Slot*>(
        mem::alloc(MemRegion::Heap, NNH_MEM_TAG_REF(meshes), sizeof(Slot) * kMeshSlots, alignof(Slot)));
    if (g_slots == nullptr) return false;
    for (int i = 0; i < kMeshSlots; ++i) g_slots[i] = Slot{};
    return true;
}

void mesh_table_shutdown() {
    if (g_slots == nullptr) return;
    for (int i = 0; i < kMeshSlots; ++i) {
        if (g_slots[i].record != nullptr) mesh_record_destroy(g_slots[i].record);
    }
    mem::free(g_slots);
    g_slots = nullptr;
}

bool mesh_table_ready() {
    gfx3d_link(); // a game that creates meshes links the 3D hook's registrar
    return g_slots != nullptr;
}

MeshRecord* mesh_get(int slot, std::uint32_t generation) {
    if (g_slots == nullptr || slot < 0 || slot >= kMeshSlots) return nullptr;
    const Slot& s = g_slots[slot];
    return s.record != nullptr && s.generation == generation ? s.record : nullptr;
}

int mesh_add(MeshRecord* record, std::uint32_t& generation) {
    if (g_slots == nullptr) return -1;
    for (int i = 0; i < kMeshSlots; ++i) {
        if (g_slots[i].record != nullptr) continue;
        g_slots[i] = Slot{record, g_next_generation++, kLive};
        generation = g_slots[i].generation;
        return i;
    }
    return -1;
}

void mesh_release(int slot, std::uint32_t generation) {
    if (mesh_get(slot, generation) == nullptr) return;
    Slot& s = g_slots[slot];
    if (s.stage != kLive) return;
    if (gpu_may_use_resources()) {
        s.stage = kReleased; // draws recorded this frame, or a frame the GPU still draws, still find it
        return;
    }
    mesh_record_destroy(s.record);
    s = Slot{};
}

void mesh_retire_released() {
    if (g_slots == nullptr) return;
    for (int i = 0; i < kMeshSlots; ++i) {
        if (g_slots[i].stage == kReleased) g_slots[i].stage = kRetired;
    }
}

void mesh_free_retired() {
    if (g_slots == nullptr) return;
    for (int i = 0; i < kMeshSlots; ++i) {
        if (g_slots[i].stage != kRetired) continue;
        mesh_record_destroy(g_slots[i].record);
        g_slots[i] = Slot{};
    }
}

int mesh_count() {
    int n = 0;
    for (int i = 0; g_slots != nullptr && i < kMeshSlots; ++i) n += g_slots[i].record != nullptr ? 1 : 0;
    return n;
}

std::size_t mesh_bytes() {
    std::size_t n = 0;
    for (int i = 0; g_slots != nullptr && i < kMeshSlots; ++i)
        if (const MeshRecord* r = g_slots[i].record) n += r->block_bytes + r->linear_bytes;
    return n;
}

} // namespace detail
} // namespace nnh
