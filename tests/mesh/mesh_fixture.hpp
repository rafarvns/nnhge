#pragma once
// Shared mesh test data (spec 026 stage 2): a quad of two triangles and a helper that builds bytes from a MeshDesc.

#include <cstdint>
#include <vector>

#include "mesh/mesh_build.hpp"
#include "mesh/mesh_format.hpp"
#include "mesh/mesh_validate.hpp"
#include "test.hpp"

namespace mesh_test {

inline const nnh::Vec3 kQuadPositions[4] = {{-1, 0, -1}, {1, 0, -1}, {1, 0, 1}, {-1, 0, 1}};
inline const nnh::Vec3 kQuadNormals[4] = {{0, 1, 0}, {0, 1, 0}, {0, 1, 0}, {0, 1, 0}};
inline const nnh::Vec2 kQuadUvs[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
inline const std::uint16_t kQuadIndices[6] = {0, 2, 1, 0, 3, 2};

inline nnh::MeshDesc quad_desc() {
    return {.positions = kQuadPositions, .vertex_count = 4, .normals = kQuadNormals, .uvs = kQuadUvs,
            .indices = kQuadIndices, .index_count = 6};
}

/// The file built from `desc` (empty when it doesn't measure ok).
inline std::vector<std::uint8_t> build(const nnh::MeshDesc& desc, const nnh::detail::mesh::BuildExtras& extras = {}) {
    const nnh::detail::mesh::BuildInfo info = nnh::detail::mesh::measure(desc, extras);
    if (!info.ok) return {};
    std::vector<std::uint8_t> bytes(info.size);
    CHECK(nnh::detail::mesh::write(desc, extras, bytes.data(), bytes.size()));
    return bytes;
}

} // namespace mesh_test
