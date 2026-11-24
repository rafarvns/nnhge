// One draw through the rasteriser (raster3d.hpp): vertices → clipping → coverage → depth test → shading → cut-out or
// blending → the colour and depth buffers. Depth: NDC z, less-than, written by opaque and cut-out draws only.

#include "raster3d.hpp"

namespace nnh::raster3d {
namespace {

struct DrawContext {
    const Target* target;
    const platform::Pass3D* pass;
    const LightTables* tables;
    const platform::Material3DGpu* material;
    const TextureView* texture;
    bool has_colors;
};

void pixel(void* ctx, int x, int y, float z, const float* attr) {
    const DrawContext& c = *static_cast<const DrawContext*>(ctx);
    const std::size_t i = static_cast<std::size_t>(y) * static_cast<std::size_t>(c.target->width) + x;
    float* depth = c.target->depth + i;
    if (!(z < *depth)) return;
    float rgba[4];
    shade(*c.pass, *c.tables, *c.material, c.has_colors, *c.texture, attr, rgba);
    std::uint8_t* dst = c.target->rgba + i * 4;
    const std::uint8_t a = to_byte(rgba[3]);
    if (c.material->alpha == 1 && a < c.material->cutoff) return; // cut-out
    if (c.material->alpha == 2) {                                 // blend, no depth write
        for (int k = 0; k < 3; ++k) {
            const unsigned s = to_byte(rgba[k]);
            dst[k] = static_cast<std::uint8_t>((s * a + dst[k] * (255u - a) + 127u) / 255u);
        }
        return;
    }
    for (int k = 0; k < 3; ++k) dst[k] = to_byte(rgba[k]);
    dst[3] = 255;
    *depth = z;
}

} // namespace

void clear(const Target& t, Color c) {
    const std::size_t n = static_cast<std::size_t>(t.width) * static_cast<std::size_t>(t.height);
    for (std::size_t i = 0; i < n; ++i) {
        t.rgba[i * 4] = c.r, t.rgba[i * 4 + 1] = c.g, t.rgba[i * 4 + 2] = c.b, t.rgba[i * 4 + 3] = 255;
        t.depth[i] = 1.0f;
    }
}

void clear_depth(const Target& t) {
    const std::size_t n = static_cast<std::size_t>(t.width) * static_cast<std::size_t>(t.height);
    for (std::size_t i = 0; i < n; ++i) t.depth[i] = 1.0f;
}

void draw(const Target& target, const platform::Pass3D& pass, const LightTables& tables, const platform::Draw3D& d,
          const MeshView& mesh, const TextureView& texture) {
    DrawContext ctx{&target, &pass, &tables, &d.material, &texture, (mesh.flags & 2) != 0};
    const TextureView none{};
    if (d.material.texture == 0) ctx.texture = &none;
    const PixelSink sink{&pixel, &ctx};
    const std::uint32_t end = d.first_index + d.index_count;
    for (std::uint32_t i = d.first_index; i + 2 < end + 0 && i + 2 < mesh.index_count; i += 3) {
        const ClipVertex in[3] = {vertex_stage(pass, d, mesh, mesh.indices[i]),
                                  vertex_stage(pass, d, mesh, mesh.indices[i + 1]),
                                  vertex_stage(pass, d, mesh, mesh.indices[i + 2])};
        ClipVertex poly[9];
        const int n = clip_triangle(in, poly);
        if (n < 3) continue;
        ScreenVertex s[9];
        for (int k = 0; k < n; ++k) s[k] = to_screen(poly[k], target.width, target.height);
        for (int k = 1; k + 1 < n; ++k) rasterize(s[0], s[k], s[k + 1], d.material.cull_back, target.width,
                                                  target.height, sink);
    }
}

} // namespace nnh::raster3d
