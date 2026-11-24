#pragma once
// Meshes at run time (spec 026 FR-8, plan P6): the mesh table (256 slots, generations), a mesh's record (one heap block:
// counts, decode, bounds, submeshes with their materials, owned textures), and access for the 3D pass. Internal.

#include <cstddef>
#include <cstdint>

#include "mesh/mesh_validate.hpp"
#include "nnh/memory.hpp"
#include "nnh/mesh.hpp"
#include "nnh/platform/video3d.hpp"

namespace nnh {

NNH_MEM_TAG_EXTERN(meshes);

struct MeshAccess {
    static int slot(const Mesh& m) { return m.slot_; }
    static std::uint32_t generation(const Mesh& m) { return m.generation_; }
    static Mesh make(int slot, std::uint32_t generation) {
        Mesh m;
        m.slot_ = slot;
        m.generation_ = generation;
        return m;
    }
};

namespace detail {

inline constexpr int kMeshSlots = 256;

struct SubmeshRuntime {
    std::uint32_t first_index = 0;
    std::uint32_t index_count = 0;
    float uv_scale[2] = {0, 0};
    float uv_offset[2] = {0, 0};
    Material3D material{};
};

/// One mesh: a heap block of this header, then `submesh_count` SubmeshRuntime, then `texture_count` Texture.
struct MeshRecord {
    platform::MeshHandle handle = 0;
    std::size_t linear_bytes = 0; ///< accounted Linear/meshes
    std::size_t block_bytes = 0;  ///< this block (Heap/meshes)
    int vertex_count = 0;
    int index_count = 0;
    int submesh_count = 0;
    int texture_count = 0;
    float pos_scale[3] = {1, 1, 1};
    float pos_offset[3] = {0, 0, 0};
    Aabb3 bounds{};
    Sphere sphere{};
    SubmeshRuntime* submeshes = nullptr;
    Texture* textures = nullptr; ///< owned (load_mesh)
};

/// The table: ready only with 3D on (gfx3d init). Single-threaded.
bool mesh_table_init();
/// Destroys every live mesh (their handles go stale) and frees the table.
void mesh_table_shutdown();
bool mesh_table_ready();
/// A live record, or nullptr for a stale or invalid handle.
MeshRecord* mesh_get(int slot, std::uint32_t generation);
/// The record of a valid Mesh, or nullptr.
inline MeshRecord* mesh_get(const Mesh& m) { return mesh_get(MeshAccess::slot(m), MeshAccess::generation(m)); }
/// Adds a record (owned from now on). @return the slot (−1 when the table is full) and its generation.
int mesh_add(MeshRecord* record, std::uint32_t& generation);
/// Releases a mesh: freed now, or after the frame when a frame is open (draws recorded this frame may use it).
void mesh_release(int slot, std::uint32_t generation);
/// Frees the meshes released during the frame (gfx3d's after_frame).
void mesh_flush_released();
/// Live meshes and their bytes (heap blocks + linear), for stats.
int mesh_count();
std::size_t mesh_bytes();

/// Builds a record from a validated file: the block, the platform mesh, accounting. Textures are left empty (the
/// caller fills `textures` and the materials' TextureIds). @return nullptr and an Error on failure.
MeshRecord* mesh_record_create(const mesh::MeshView& view, const char* what, Error& error);
/// Frees a record's platform mesh, owned textures, accounting and block.
void mesh_record_destroy(MeshRecord* record);
/// The Material3D a file's material record describes (texture left empty).
Material3D material_from_record(const mesh::MaterialRecord& r);
/// Adds a built record to the table (or destroys it when full) and returns its handle (create_mesh.cpp).
Result<Mesh> mesh_publish(MeshRecord* record, const char* what);
/// The `Unsupported` error (and log line) of a mesh call with 3D off.
Error mesh_3d_off(const char* what);

/// Referenced by every mesh and 3D entry point so that linking them links gfx3d/gfx3d_ops.cpp, whose registrar sets
/// the 3D hook (plan P14).
void gfx3d_link();

} // namespace detail
} // namespace nnh
