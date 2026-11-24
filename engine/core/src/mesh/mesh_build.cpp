// MeshDesc → .nnhmesh bytes (mesh_build.hpp): bounds, quantisation, flat normals when missing, submesh records, CRC.

#include "mesh/mesh_build.hpp"

#include <cstring>

#include "core/crc32.hpp"
#include "mesh/mesh_format.hpp"
#include "mesh/mesh_quantize.hpp"

namespace nnh::detail::mesh {
namespace {

struct Ctx {
    const MeshDesc& d;
    const BuildInfo& info;
    Axis pos[3];
    Axis uv[2];
};

int source_index(const MeshDesc& d, int i) { return d.indices != nullptr ? d.indices[i] : i; }

/// The source vertex of output vertex `v` (unwelded: the index at position v).
int source_vertex(const Ctx& c, int v) { return c.info.flat ? source_index(c.d, v) : v; }

void bounds_axes(Ctx& c) {
    Vec3 lo = c.d.positions[0], hi = lo;
    for (int v = 1; v < c.d.vertex_count; ++v) {
        const Vec3 p = c.d.positions[v];
        lo = {p.x < lo.x ? p.x : lo.x, p.y < lo.y ? p.y : lo.y, p.z < lo.z ? p.z : lo.z};
        hi = {p.x > hi.x ? p.x : hi.x, p.y > hi.y ? p.y : hi.y, p.z > hi.z ? p.z : hi.z};
    }
    c.pos[0] = axis_for(lo.x, hi.x), c.pos[1] = axis_for(lo.y, hi.y), c.pos[2] = axis_for(lo.z, hi.z);
    if (c.d.uvs == nullptr) return;
    Vec2 ulo = c.d.uvs[0], uhi = ulo;
    for (int v = 1; v < c.d.vertex_count; ++v) {
        const Vec2 u = c.d.uvs[v];
        ulo = {u.x < ulo.x ? u.x : ulo.x, u.y < ulo.y ? u.y : ulo.y};
        uhi = {u.x > uhi.x ? u.x : uhi.x, u.y > uhi.y ? u.y : uhi.y};
    }
    c.uv[0] = axis_for(ulo.x, uhi.x), c.uv[1] = axis_for(ulo.y, uhi.y);
}

/// Writes the vertices; returns the decoded bounds (box and sphere) of what the GPU will draw.
void write_vertices(const Ctx& c, std::uint8_t flags, std::uint8_t* out, MeshHeader& h) {
    const int stride = vertex_stride(flags);
    const int uv_at = uv_offset_in_vertex(flags), color_at = color_offset_in_vertex(flags);
    Vec3 lo{}, hi{};
    for (int v = 0; v < c.info.vertex_count; ++v) {
        std::uint8_t* dst = out + static_cast<std::size_t>(v) * static_cast<std::size_t>(stride);
        const int s = source_vertex(c, v);
        const Vec3 p = c.d.positions[s];
        const std::int16_t q[4] = {quantize(p.x, c.pos[0]), quantize(p.y, c.pos[1]), quantize(p.z, c.pos[2]), 0};
        std::memcpy(dst, q, 8);
        Vec3 n;
        if (c.info.flat) {
            const int t = v - v % 3;
            n = face_normal(c.d.positions[source_index(c.d, t)], c.d.positions[source_index(c.d, t + 1)],
                            c.d.positions[source_index(c.d, t + 2)]);
        } else {
            n = c.d.normals[s];
        }
        std::int8_t qn[4] = {0, 0, 0, 0};
        quantize_normal(n, qn);
        std::memcpy(dst + 8, qn, 4);
        if (uv_at >= 0) {
            const std::int16_t qu[2] = {quantize(c.d.uvs[s].x, c.uv[0]), quantize(c.d.uvs[s].y, c.uv[1])};
            std::memcpy(dst + uv_at, qu, 4);
        }
        if (color_at >= 0) {
            const Color col = c.d.colors[s];
            const std::uint8_t rgba[4] = {col.r, col.g, col.b, col.a};
            std::memcpy(dst + color_at, rgba, 4);
        }
        const Vec3 dp{dequantize(q[0], c.pos[0]), dequantize(q[1], c.pos[1]), dequantize(q[2], c.pos[2])};
        if (v == 0) lo = hi = dp;
        lo = {dp.x < lo.x ? dp.x : lo.x, dp.y < lo.y ? dp.y : lo.y, dp.z < lo.z ? dp.z : lo.z};
        hi = {dp.x > hi.x ? dp.x : hi.x, dp.y > hi.y ? dp.y : hi.y, dp.z > hi.z ? dp.z : hi.z};
    }
    const Vec3 centre{c.pos[0].offset, c.pos[1].offset, c.pos[2].offset};
    float r2 = 0;
    for (int v = 0; v < c.info.vertex_count; ++v) {
        std::int16_t qv[3];
        std::memcpy(qv, out + static_cast<std::size_t>(v) * static_cast<std::size_t>(stride), 6);
        const Vec3 d{dequantize(qv[0], c.pos[0]) - centre.x, dequantize(qv[1], c.pos[1]) - centre.y,
                     dequantize(qv[2], c.pos[2]) - centre.z};
        const float l2 = (d.x * d.x + d.y * d.y) + d.z * d.z;
        r2 = l2 > r2 ? l2 : r2;
    }
    const float lo3[3] = {lo.x, lo.y, lo.z}, hi3[3] = {hi.x, hi.y, hi.z};
    std::memcpy(h.aabb_min, lo3, 12);
    std::memcpy(h.aabb_max, hi3, 12);
    h.sphere[0] = centre.x, h.sphere[1] = centre.y, h.sphere[2] = centre.z;
    h.sphere[3] = __builtin_sqrtf(r2);
}

MaterialRecord material_record(const Material3D& m, std::uint8_t texture) {
    MaterialRecord r{};
    r.base[0] = m.base_color.r, r.base[1] = m.base_color.g, r.base[2] = m.base_color.b, r.base[3] = m.base_color.a;
    r.specular[0] = m.specular.r, r.specular[1] = m.specular.g, r.specular[2] = m.specular.b;
    r.shininess = m.shininess;
    r.texture = texture;
    r.alpha = static_cast<std::uint8_t>(m.alpha);
    r.cutoff = m.alpha_cutoff;
    r.filter = static_cast<std::uint8_t>(m.filter);
    r.wrap_u = static_cast<std::uint8_t>(m.wrap_u);
    r.wrap_v = static_cast<std::uint8_t>(m.wrap_v);
    r.flags = static_cast<std::uint8_t>((m.lit ? kMatLit : 0) | (m.vertex_colors ? kMatVertexColors : 0) |
                                        (m.cull_back ? kMatCullBack : 0));
    return r;
}

void write_submeshes(const Ctx& c, const BuildExtras& e, const std::uint16_t* indices, std::uint8_t* out) {
    for (int s = 0; s < c.info.submesh_count; ++s) {
        const SubmeshDesc whole{0, c.info.index_count, Material3D{}};
        const SubmeshDesc& sm = c.d.submeshes != nullptr ? c.d.submeshes[s] : whole;
        SubmeshRecord r{};
        r.first_index = static_cast<std::uint32_t>(sm.first_index);
        r.index_count = static_cast<std::uint32_t>(sm.index_count);
        std::uint16_t lo = 0xFFFF, hi = 0;
        for (int i = sm.first_index; i < sm.first_index + sm.index_count; ++i) {
            lo = indices[i] < lo ? indices[i] : lo;
            hi = indices[i] > hi ? indices[i] : hi;
        }
        r.min_vertex = lo, r.max_vertex = hi;
        r.uv_scale[0] = c.uv[0].scale, r.uv_scale[1] = c.uv[1].scale;
        r.uv_offset[0] = c.uv[0].offset, r.uv_offset[1] = c.uv[1].offset;
        r.material = material_record(sm.material, e.submesh_textures != nullptr ? e.submesh_textures[s] : kNoTexture);
        std::memcpy(out + static_cast<std::size_t>(s) * sizeof r, &r, sizeof r);
    }
}

} // namespace

bool write(const MeshDesc& d, const BuildExtras& e, std::uint8_t* out, std::size_t size) {
    const BuildInfo info = measure(d, e);
    if (!info.ok || size != info.size || out == nullptr) return false;
    std::memset(out, 0, size);
    Ctx c{d, info, {}, {}};
    bounds_axes(c);
    const std::uint8_t flags = static_cast<std::uint8_t>((d.uvs != nullptr ? kFlagUv : 0) |
                                                         (d.colors != nullptr ? kFlagColor : 0));
    const int stride = vertex_stride(flags);
    const Layout l = layout(info.submesh_count, e.texture_count, info.vertex_count, stride, info.index_count);
    MeshHeader h{};
    std::memcpy(h.magic, kMagic, 4);
    h.version = kVersion, h.header_size = sizeof(MeshHeader), h.file_size = l.file_size;
    h.flags = flags, h.submesh_count = static_cast<std::uint8_t>(info.submesh_count);
    h.texture_count = static_cast<std::uint8_t>(e.texture_count), h.stride = static_cast<std::uint8_t>(stride);
    h.vertex_count = static_cast<std::uint32_t>(info.vertex_count);
    h.index_count = static_cast<std::uint32_t>(info.index_count);
    for (int a = 0; a < 3; ++a) h.pos_scale[a] = c.pos[a].scale, h.pos_offset[a] = c.pos[a].offset;
    std::memcpy(h.offsets, l.offsets, sizeof h.offsets);
    write_vertices(c, flags, out + l.offsets[2], h);
    auto* indices = reinterpret_cast<std::uint16_t*>(out + l.offsets[3]);
    for (int i = 0; i < info.index_count; ++i)
        indices[i] = static_cast<std::uint16_t>(info.flat ? i : source_index(d, i));
    write_submeshes(c, e, indices, out + l.offsets[0]);
    for (int t = 0; t < e.texture_count; ++t)
        std::memcpy(out + l.offsets[1] + t * kPathBytes, e.texture_paths[t], std::strlen(e.texture_paths[t]));
    std::memcpy(out, &h, sizeof h);
    h.crc = crc32(out + sizeof h, size - sizeof h);
    std::memcpy(out, &h, sizeof h);
    return true;
}

} // namespace nnh::detail::mesh
