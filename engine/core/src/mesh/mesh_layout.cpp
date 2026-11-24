// The .nnhmesh section layout (mesh_format.hpp): sections in order, each 4-byte aligned.

#include "mesh/mesh_format.hpp"

namespace nnh::detail::mesh {

Layout layout(int submesh_count, int texture_count, int vertex_count, int stride, int index_count) {
    Layout l;
    std::uint32_t at = sizeof(MeshHeader);
    l.offsets[0] = at;
    at += static_cast<std::uint32_t>(submesh_count) * sizeof(SubmeshRecord);
    l.offsets[1] = at;
    at += static_cast<std::uint32_t>(texture_count) * kPathBytes;
    l.offsets[2] = at;
    at += static_cast<std::uint32_t>(vertex_count) * static_cast<std::uint32_t>(stride);
    l.offsets[3] = at;
    at += static_cast<std::uint32_t>(index_count) * 2u;
    at = (at + 3u) & ~3u; // the index section ends on a 4-byte boundary
    l.file_size = at;
    return l;
}

} // namespace nnh::detail::mesh
