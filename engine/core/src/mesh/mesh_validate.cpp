// The .nnhmesh reader's checks (mesh_validate.hpp). Reads through memcpy (no alignment assumed); integer limits are
// checked before any multiplication can overflow.

#include "mesh/mesh_validate.hpp"

#include <bit>
#include <cstdio>
#include <cstring>

#include "core/crc32.hpp"
#include "mesh/mesh_build.hpp"

namespace nnh::detail::mesh {
namespace {

bool finite(float f) { return ((std::bit_cast<std::uint32_t>(f) >> 23) & 0xFFu) != 0xFFu; }

bool finite_all(const float* f, int n) {
    for (int i = 0; i < n; ++i)
        if (!finite(f[i])) return false;
    return true;
}

bool check_header(const MeshHeader& h, std::size_t size, const char*& why) {
    if (std::memcmp(h.magic, kMagic, 4) != 0) return why = "not a mesh (magic)", false;
    if (h.version != kVersion) return why = "unsupported version", false;
    if (h.header_size != sizeof(MeshHeader) || h.file_size != size) return why = "wrong sizes", false;
    if (h.flags > (kFlagUv | kFlagColor) || h.stride != vertex_stride(h.flags)) return why = "bad flags or stride", false;
    if (h.vertex_count < 1 || h.vertex_count > 65535) return why = "vertex count out of range", false;
    if (h.index_count < 3 || h.index_count > static_cast<std::uint32_t>(kMaxIndices) || h.index_count % 3 != 0)
        return why = "index count out of range", false;
    if (h.submesh_count < 1 || h.submesh_count > kMaxSubmeshes) return why = "submesh count out of range", false;
    if (h.texture_count > kMaxTextures) return why = "too many textures", false;
    const Layout l = layout(h.submesh_count, h.texture_count, static_cast<int>(h.vertex_count), h.stride,
                            static_cast<int>(h.index_count));
    if (l.file_size != size || std::memcmp(l.offsets, h.offsets, sizeof h.offsets) != 0)
        return why = "wrong layout", false;
    if (!finite_all(h.pos_scale, 3) || !finite_all(h.pos_offset, 3) || !finite_all(h.aabb_min, 3) ||
        !finite_all(h.aabb_max, 3) || !finite_all(h.sphere, 4))
        return why = "non-finite bounds", false;
    for (int a = 0; a < 3; ++a)
        if (!(h.pos_scale[a] > 0.0f) || h.aabb_min[a] > h.aabb_max[a]) return why = "bad scale or box", false;
    if (!(h.sphere[3] >= 0.0f)) return why = "bad sphere", false;
    return true;
}

bool check_submesh(const SubmeshRecord& s, const MeshHeader& h, const char*& why) {
    if (s.first_index % 3 != 0 || s.index_count < 3 || s.index_count % 3 != 0 || s.first_index > h.index_count ||
        s.index_count > h.index_count - s.first_index)
        return why = "submesh range out of the indices", false;
    if (s.min_vertex > s.max_vertex || s.max_vertex >= h.vertex_count) return why = "submesh vertex range", false;
    if (!finite_all(s.uv_scale, 2) || !finite_all(s.uv_offset, 2)) return why = "non-finite uv decode", false;
    const MaterialRecord& m = s.material;
    if (m.shininess > 7 || m.alpha > 2 || m.filter > 1 || m.wrap_u > 2 || m.wrap_v > 2 || m.flags > 7)
        return why = "invalid material", false;
    if (m.texture != kNoTexture && m.texture >= h.texture_count) return why = "texture index out of range", false;
    return true;
}

} // namespace

SubmeshRecord submesh(const MeshView& view, int i) {
    SubmeshRecord r;
    std::memcpy(&r, view.submeshes + static_cast<std::size_t>(i) * sizeof r, sizeof r);
    return r;
}

bool validate(const void* data, std::size_t size, MeshView& out, char* why_out, std::size_t why_size) {
    const char* why = nullptr;
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    MeshView v;
    if (data == nullptr || size < sizeof(MeshHeader) || size > 0x7FFFFFFFu) {
        why = "too small";
    } else {
        std::memcpy(&v.header, bytes, sizeof v.header);
        const MeshHeader& h = v.header;
        if (check_header(h, size, why)) {
            if (crc32(bytes + sizeof h, size - sizeof h) != h.crc) why = "checksum mismatch";
            v.submeshes = bytes + h.offsets[0];
            v.textures = reinterpret_cast<const char*>(bytes + h.offsets[1]);
            v.vertices = bytes + h.offsets[2];
            v.indices = bytes + h.offsets[3];
            for (int s = 0; why == nullptr && s < h.submesh_count; ++s) check_submesh(submesh(v, s), h, why);
            for (int t = 0; why == nullptr && t < h.texture_count; ++t) {
                const char* p = v.textures + t * kPathBytes;
                if (std::memchr(p, '\0', kPathBytes) == nullptr || !valid_texture_path(p)) why = "bad texture path";
            }
            for (std::uint32_t i = 0; why == nullptr && i < h.index_count; ++i) {
                std::uint16_t index;
                std::memcpy(&index, v.indices + 2 * i, 2);
                if (index >= h.vertex_count) why = "index out of range";
            }
        }
    }
    if (why != nullptr) {
        if (why_out != nullptr && why_size > 0) std::snprintf(why_out, why_size, "%s", why);
        return false;
    }
    out = v;
    return true;
}

} // namespace nnh::detail::mesh
