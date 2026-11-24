// The mesh table (mesh_internal.hpp): 256 slots with generations, released meshes kept until the frame ends.

#include "core/engine_state.hpp"
#include "memory/memory_internal.hpp"
#include "mesh/mesh_internal.hpp"

namespace nnh {

NNH_MEM_TAG(meshes);

namespace detail {
namespace {

struct Slot {
    MeshRecord* record = nullptr;
    std::uint32_t generation = 0;
    bool released = false; ///< freed at the end of the frame
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
        g_slots[i] = Slot{record, g_next_generation++, false};
        generation = g_slots[i].generation;
        return i;
    }
    return -1;
}

void mesh_release(int slot, std::uint32_t generation) {
    if (mesh_get(slot, generation) == nullptr) return;
    Slot& s = g_slots[slot];
    if (s.released) return;
    if (frame_open()) {
        s.released = true; // draws recorded this frame still find it; freed in mesh_flush_released()
        return;
    }
    mesh_record_destroy(s.record);
    s = Slot{};
}

void mesh_flush_released() {
    if (g_slots == nullptr) return;
    for (int i = 0; i < kMeshSlots; ++i) {
        if (!g_slots[i].released) continue;
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
