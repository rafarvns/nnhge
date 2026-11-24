// Recording 3D draws (nnh/gfx3d.hpp draw, FR-17/FR-29): checks, the model's rows and the world bounding sphere (for
// culling at submit), O(1), no allocation. Misuse asserts in Debug/Development and records nothing in Release.

#include <bit>

#include "gfx/gfx_state.hpp"
#include "gfx3d/gfx3d_state.hpp"
#include "mesh/mesh_internal.hpp"
#include "nnh/assert.hpp"

namespace nnh {
namespace {

bool finite_bits(float f) { return (std::bit_cast<std::uint32_t>(f) & 0x7F800000u) != 0x7F800000u; }

/// Finite with an exact (0, 0, 0, 1) bottom row.
bool affine(const Mat4& m) {
    for (const float f : m.m)
        if (!finite_bits(f)) return false;
    return m.m[3] == 0.0f && m.m[7] == 0.0f && m.m[11] == 0.0f && m.m[15] == 1.0f;
}

/// The mesh's sphere under `m`: centre transformed, radius × the largest column length.
Sphere world_sphere(const Mat4& m, const Sphere& s) {
    const Vec3 c = transform_point(m, s.center);
    float k = 0;
    for (int col = 0; col < 3; ++col) {
        const Vec3 v{m.m[col * 4], m.m[col * 4 + 1], m.m[col * 4 + 2]};
        const float l2 = dot(v, v);
        k = l2 > k ? l2 : k;
    }
    return {c, s.radius * __builtin_sqrtf(k)};
}

} // namespace

namespace gfx3d {

void draw(Screen screen, const Mesh& mesh, const Mat4& model, const DrawOptions3D& options) {
    detail::gfx3d_link();
    detail::State3D& g = detail::state3d();
    if (!g.ready) {
        NNH_ASSERT(false, "gfx3d::draw(): 3D is off (set AppConfig::gfx.max_draws_3d)");
        return;
    }
    if (!detail::gfx_drawing()) {
        NNH_ASSERT(false, "gfx3d::draw() outside a frame");
        return;
    }
    if (screen != Screen::Top) {
        NNH_ASSERT(false, "gfx3d::draw(): only the top screen draws 3D");
        return;
    }
    const detail::MeshRecord* r = detail::mesh_get(mesh);
    if (r == nullptr) {
        NNH_ASSERT(false, "gfx3d::draw(): an empty or destroyed mesh");
        return;
    }
    if (options.submesh < -1 || options.submesh >= r->submesh_count) {
        NNH_ASSERT(false, "gfx3d::draw(): submesh %d of %d", options.submesh, r->submesh_count);
        return;
    }
    if (!affine(model)) {
        NNH_ASSERT(false, "gfx3d::draw(): the model matrix isn't finite and affine");
        return;
    }
    if (g.count >= g.max_draws) {
        ++g.dropped;
        NNH_ASSERT(false, "more than %d 3D draws in a frame: raise AppConfig::gfx.max_draws_3d", g.max_draws);
        return;
    }
    detail::DrawRecord& d = g.records[g.count];
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 4; ++col) d.model[row * 4 + col] = model.m[col * 4 + row];
    d.flags = 0;
    if (options.material != nullptr) {
        d.material = *options.material;
        const bool valid = detail::check_material(d.material);
        NNH_ASSERT(valid, "gfx3d::draw(): a material value is out of range (clamped)");
        d.flags = detail::kRecordHasMaterial;
    }
    d.mesh_slot = static_cast<std::int16_t>(MeshAccess::slot(mesh));
    d.mesh_generation = MeshAccess::generation(mesh);
    d.submesh = static_cast<std::int8_t>(options.submesh);
    d.tint = options.tint;
    d.call = static_cast<std::uint16_t>(g.count);
    d.reserved = 0;
    g.spheres[g.count] = world_sphere(model, r->sphere);
    ++g.count;
}

void draw(Screen screen, const Mesh& mesh, const Transform3D& transform, const DrawOptions3D& options) {
    draw(screen, mesh, to_mat4(transform), options);
}

} // namespace gfx3d
} // namespace nnh
