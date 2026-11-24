// One platform draw (pass3d.hpp, FR-10/FR-17): the model-view with the mesh's quantisation decode folded in (the GPU
// reads raw int16 positions), the normal matrix (inverse transpose by cofactors over the determinant), the uv decode,
// the material resolved with the tint and the texture's platform handle. -ffp-contract=off.

#include "gfx/gfx_state.hpp"
#include "gfx3d/pass3d.hpp"
#include "mesh/mesh_internal.hpp"
#include "nnh/assert.hpp"

namespace nnh::detail {
namespace {

Mat4 model_of(const DrawRecord& r) {
    Mat4 m;
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 4; ++col) m.m[col * 4 + row] = r.model[row * 4 + col];
    return m;
}

void normal_matrix(const Mat4& m, float out[9]) {
    auto a = [&m](int r, int c) { return m.m[c * 4 + r]; };
    float c[9]; // cofactors, row-major: c[r * 3 + k] = C(r, k)
    c[0] = a(1, 1) * a(2, 2) - a(2, 1) * a(1, 2);
    c[1] = a(2, 0) * a(1, 2) - a(1, 0) * a(2, 2);
    c[2] = a(1, 0) * a(2, 1) - a(2, 0) * a(1, 1);
    c[3] = a(2, 1) * a(0, 2) - a(0, 1) * a(2, 2);
    c[4] = a(0, 0) * a(2, 2) - a(2, 0) * a(0, 2);
    c[5] = a(2, 0) * a(0, 1) - a(0, 0) * a(2, 1);
    c[6] = a(0, 1) * a(1, 2) - a(1, 1) * a(0, 2);
    c[7] = a(1, 0) * a(0, 2) - a(0, 0) * a(1, 2);
    c[8] = a(0, 0) * a(1, 1) - a(1, 0) * a(0, 1);
    const float det = (a(0, 0) * c[0] + a(0, 1) * c[1]) + a(0, 2) * c[2];
    const bool usable = det > 1e-30f || det < -1e-30f;
    for (int i = 0; i < 9; ++i) out[i] = usable ? c[i] / det : (i % 4 == 0 ? 1.0f : 0.0f);
}

float unit(std::uint8_t v) { return static_cast<float>(v) / 255.0f; }

} // namespace

const Material3D& draw_material(const DrawRecord& r, const MeshRecord& mesh, int submesh) {
    return (r.flags & kRecordHasMaterial) != 0 ? r.material : mesh.submeshes[submesh].material;
}

bool make_draw(const DrawRecord& r, const MeshRecord& mesh, int submesh, const Mat4& view, platform::Draw3D& d) {
    const SubmeshRuntime& sm = mesh.submeshes[submesh];
    const Mat4 mv = view * model_of(r);
    const float s[3] = {mesh.pos_scale[0], mesh.pos_scale[1], mesh.pos_scale[2]};
    const Vec4 t = mv * Vec4{mesh.pos_offset[0], mesh.pos_offset[1], mesh.pos_offset[2], 1};
    const float tc[3] = {t.x, t.y, t.z};
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) d.model_view[row * 4 + col] = mv.m[col * 4 + row] * s[col];
        d.model_view[row * 4 + 3] = tc[row];
    }
    normal_matrix(mv, d.normal);
    d.mesh = mesh.handle;
    d.first_index = sm.first_index;
    d.index_count = sm.index_count;
    d.uv_scale[0] = sm.uv_scale[0], d.uv_scale[1] = sm.uv_scale[1];
    d.uv_offset[0] = sm.uv_offset[0], d.uv_offset[1] = sm.uv_offset[1];
    const Material3D& m = draw_material(r, mesh, submesh);
    platform::Material3DGpu& g = d.material;
    g.base[0] = unit(m.base_color.r) * unit(r.tint.r);
    g.base[1] = unit(m.base_color.g) * unit(r.tint.g);
    g.base[2] = unit(m.base_color.b) * unit(r.tint.b);
    g.base[3] = unit(m.base_color.a) * unit(r.tint.a);
    g.specular[0] = unit(m.specular.r), g.specular[1] = unit(m.specular.g), g.specular[2] = unit(m.specular.b);
    g.shininess = m.shininess;
    g.alpha = static_cast<std::uint8_t>(m.alpha);
    g.cutoff = m.alpha_cutoff;
    g.filter = static_cast<std::uint8_t>(m.filter);
    g.wrap_u = static_cast<std::uint8_t>(m.wrap_u);
    g.wrap_v = static_cast<std::uint8_t>(m.wrap_v);
    g.lit = m.lit, g.vertex_colors = m.vertex_colors, g.cull_back = m.cull_back;
    g.texture = 0;
    if (m.texture.slot >= 0) {
        const TextureData* tex = gfx_state().textures.get(m.texture.slot, m.texture.generation);
        NNH_ASSERT(tex != nullptr, "a 3D material uses a texture destroyed earlier (drawn untextured)");
        if (tex != nullptr) g.texture = tex->platform;
    }
    return true;
}

} // namespace nnh::detail
