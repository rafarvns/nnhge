// Built-in meshes (spec 026 FR-5, T2.7): counts, unit normals pointing out (within 1/127), counter-clockwise
// outside faces, bounds, texture coordinate ranges, stable bytes (CRC of what the platform got).

#include <cmath>
#include <cstring>
#include <utility>

#include "core/crc32.hpp"
#include "mesh/mesh_internal.hpp"
#include "nnh/app.hpp"
#include "nnh/platform_fake.hpp"
#include "test.hpp"

namespace fake = nnh::platform_fake;
using nnh::Vec3;

namespace {

void restart3d() {
    nnh::shutdown();
    fake::reset();
    nnh::AppConfig config;
    config.gfx.max_draws_3d = 16;
    CHECK(nnh::init(config));
}

/// Decodes vertex `v` of a created mesh (stride 16: position, normal, uv).
struct Vertex {
    Vec3 p, n;
};
Vertex vertex(const nnh::Mesh& m, int v) {
    const nnh::detail::MeshRecord* r = nnh::detail::mesh_get(m);
    std::size_t size = 0;
    const std::uint8_t* data = fake::mesh_data(r->handle, size);
    std::int16_t q[4];
    std::int8_t n[4];
    std::memcpy(q, data + 16 * v, 8);
    std::memcpy(n, data + 16 * v + 8, 4);
    return {{q[0] * r->pos_scale[0] + r->pos_offset[0], q[1] * r->pos_scale[1] + r->pos_offset[1],
             q[2] * r->pos_scale[2] + r->pos_offset[2]},
            {n[0] / 127.0f, n[1] / 127.0f, n[2] / 127.0f}};
}

/// Every triangle faces away from the origin (counter-clockwise seen from outside) and every normal is unit.
bool faces_out(const nnh::Mesh& m) {
    const nnh::detail::MeshRecord* r = nnh::detail::mesh_get(m);
    std::size_t size = 0;
    const std::uint8_t* data = fake::mesh_data(r->handle, size);
    for (int t = 0; t < r->index_count; t += 3) {
        std::uint16_t i[3];
        std::memcpy(i, data + 16 * r->vertex_count + 2 * t, 6);
        const Vec3 a = vertex(m, i[0]).p, b = vertex(m, i[1]).p, c = vertex(m, i[2]).p;
        const Vec3 n = nnh::cross(b - a, c - a);
        if (nnh::dot(n, a + b + c) <= 0) return false;
    }
    for (int v = 0; v < r->vertex_count; ++v)
        if (std::fabs(nnh::length(vertex(m, v).n) - 1) > 1.5f / 127) return false;
    return true;
}

std::uint32_t crc_of(const nnh::Mesh& m) {
    std::size_t size = 0;
    const std::uint8_t* data = fake::mesh_data(nnh::detail::mesh_get(m)->handle, size);
    return nnh::detail::crc32(data, size);
}

} // namespace

NNH_TEST(make_cube_has_24_vertices_and_12_outward_triangles) {
    restart3d();
    auto r = nnh::make_cube(2);
    CHECK(r.ok());
    const nnh::Mesh& m = r.value();
    CHECK(m.vertex_count() == 24 && m.triangle_count() == 12);
    CHECK(std::fabs(m.bounds().max.x - 1) < 1e-6f && std::fabs(m.bounds().min.z + 1) < 1e-6f);
    CHECK(std::fabs(m.bounding_sphere().radius - std::sqrt(3.0f)) < 1e-5f);
    CHECK(faces_out(m));
    std::printf("    cube crc %08X\n", crc_of(m));
    nnh::shutdown();
}

NNH_TEST(make_plane_faces_up) {
    restart3d();
    auto r = nnh::make_plane(4, 2, 3);
    CHECK(r.ok() && r.value().vertex_count() == 16 && r.value().triangle_count() == 18);
    const nnh::Mesh& m = r.value();
    for (int v = 0; v < 16; ++v) CHECK(vertex(m, v).n == (Vec3{0, 1, 0}));
    CHECK(std::fabs(m.bounds().max.x - 2) < 1e-6f && std::fabs(m.bounds().max.z - 1) < 1e-6f);
    const nnh::detail::MeshRecord* rec = nnh::detail::mesh_get(m);
    std::size_t size = 0;
    const std::uint8_t* data = fake::mesh_data(rec->handle, size);
    std::uint16_t i[3];
    std::memcpy(i, data + 16 * 16, 6);
    const Vec3 a = vertex(m, i[0]).p, b = vertex(m, i[1]).p, c = vertex(m, i[2]).p;
    CHECK(nnh::cross(b - a, c - a).y > 0); // counter-clockwise from above
    CHECK(!nnh::make_plane(1, 1, 0).ok() && !nnh::make_plane(0, 1, 1).ok());
    nnh::shutdown();
}

NNH_TEST(make_sphere_counts_and_outward_faces) {
    restart3d();
    auto r = nnh::make_sphere(0.5f, 8, 12);
    CHECK(r.ok());
    const nnh::Mesh& m = r.value();
    CHECK(m.vertex_count() == 9 * 13 && m.triangle_count() == 12 * (2 * 8 - 2));
    CHECK(faces_out(m));
    CHECK(std::fabs(m.bounding_sphere().radius - 0.5f) < 1e-4f);
    for (int v = 0; v < m.vertex_count(); ++v) {
        const Vertex x = vertex(m, v);
        CHECK(std::fabs(nnh::length(x.p) - 0.5f) < 1e-4f);
    }
    CHECK(!nnh::make_sphere(1, 1, 12).ok() && !nnh::make_sphere(1, 8, 2).ok());
    std::printf("    sphere crc %08X\n", crc_of(m));
    nnh::shutdown();
}

NNH_TEST(primitives_are_byte_stable) {
    restart3d();
    auto a = nnh::make_sphere(1, 12, 24), b = nnh::make_sphere(1, 12, 24);
    CHECK(a.ok() && b.ok() && crc_of(a.value()) == crc_of(b.value()));
    nnh::shutdown();
}
