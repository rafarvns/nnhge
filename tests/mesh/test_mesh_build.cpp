// Mesh bytes (spec 026 FR-4/FR-5, T2.1–T2.3): layout and strides, quantisation round trips and error bounds, flat
// normals, the built file's header and records, limits and refusals with reasons.

#include <cmath>
#include <cstring>

#include "mesh_fixture.hpp"
#include "mesh/mesh_quantize.hpp"

namespace mesh = nnh::detail::mesh;
using nnh::Vec3;

NNH_TEST(mesh_layout_and_strides) {
    CHECK(mesh::vertex_stride(0) == 12 && mesh::vertex_stride(mesh::kFlagUv) == 16);
    CHECK(mesh::vertex_stride(mesh::kFlagColor) == 16 && mesh::vertex_stride(mesh::kFlagUv | mesh::kFlagColor) == 20);
    CHECK(mesh::color_offset_in_vertex(mesh::kFlagColor) == 12 && mesh::color_offset_in_vertex(3) == 16);
    const mesh::Layout l = mesh::layout(2, 1, 4, 16, 6);
    CHECK(l.offsets[0] == 112 && l.offsets[1] == 112 + 96 && l.offsets[2] == 112 + 96 + 64);
    CHECK(l.offsets[3] == 272 + 64 && l.file_size == 336 + 12);
    CHECK(mesh::layout(1, 0, 3, 12, 3).file_size == 112 + 48 + 36 + 8); // 6 index bytes padded to 8
}

NNH_TEST(mesh_quantize_round_trips_within_half_a_step) {
    const mesh::Axis a = mesh::axis_for(-3.0f, 5.0f);
    CHECK(a.offset == 1.0f && a.scale == 4.0f / 32767.0f);
    CHECK(mesh::quantize(5.0f, a) == 32767 && mesh::quantize(-3.0f, a) == -32767 && mesh::quantize(1.0f, a) == 0);
    for (int i = 0; i <= 1000; ++i) {
        const float v = -3.0f + 8.0f * static_cast<float>(i) / 1000.0f;
        CHECK(std::fabs(mesh::dequantize(mesh::quantize(v, a), a) - v) <= a.scale * 0.5f + 1e-6f);
    }
    const mesh::Axis flat = mesh::axis_for(2.0f, 2.0f); // a degenerate extent keeps a tiny positive scale
    CHECK(flat.scale > 0 && mesh::quantize(2.0f, flat) == 0 && mesh::dequantize(0, flat) == 2.0f);
}

NNH_TEST(mesh_normals_quantize_to_unit_int8) {
    std::int8_t q[3];
    mesh::quantize_normal({0, 0, -3}, q);
    CHECK(q[0] == 0 && q[1] == 0 && q[2] == -127);
    mesh::quantize_normal({1, 1, 0}, q);
    CHECK(q[0] == 90 && q[1] == 90 && q[2] == 0); // 127/√2 = 89.8
    mesh::quantize_normal({0, 0, 0}, q);
    CHECK(q[0] == 0 && q[1] == 127 && q[2] == 0);
    CHECK(mesh::face_normal({0, 0, 0}, {1, 0, 0}, {0, 1, 0}) == (Vec3{0, 0, 1})); // counter-clockwise: +z
    CHECK(mesh::face_normal({0, 0, 0}, {1, 0, 0}, {2, 0, 0}) == (Vec3{0, 1, 0})); // degenerate
}

NNH_TEST(mesh_build_writes_header_and_records) {
    const auto bytes = mesh_test::build(mesh_test::quad_desc());
    CHECK(bytes.size() == 112 + 48 + 4 * 16 + 12);
    mesh::MeshView v;
    char why[64];
    CHECK(mesh::validate(bytes.data(), bytes.size(), v, why, sizeof why));
    CHECK(v.header.vertex_count == 4 && v.header.index_count == 6 && v.header.stride == 16);
    CHECK(v.header.flags == mesh::kFlagUv && v.header.submesh_count == 1 && v.header.texture_count == 0);
    // Bounds of the decoded positions (what the GPU draws): within a rounding of the inputs.
    CHECK(std::fabs(v.header.aabb_min[0] + 1) < 1e-6f && std::fabs(v.header.aabb_max[2] - 1) < 1e-6f);
    CHECK(std::fabs(v.header.aabb_min[1]) < 1e-6f && std::fabs(v.header.sphere[3] - std::sqrt(2.0f)) < 1e-6f);
    const mesh::SubmeshRecord s = mesh::submesh(v, 0);
    CHECK(s.first_index == 0 && s.index_count == 6 && s.min_vertex == 0 && s.max_vertex == 3);
    CHECK(s.material.texture == mesh::kNoTexture && s.material.flags == 7 && s.material.shininess == 3);
    CHECK(s.uv_offset[0] == 0.5f && s.uv_scale[1] == 0.5f / 32767.0f);
    std::int16_t q[4];
    std::memcpy(q, v.vertices + 16, 8); // vertex 1: (1, 0, −1)
    CHECK(q[0] == 32767 && q[1] == 0 && q[2] == -32767 && q[3] == 0);
    std::int8_t n[4];
    std::memcpy(n, v.vertices + 16 + 8, 4);
    CHECK(n[0] == 0 && n[1] == 127 && n[2] == 0);
}

NNH_TEST(mesh_build_generates_flat_normals_unwelded) {
    nnh::MeshDesc d = mesh_test::quad_desc();
    d.normals = nullptr;
    const auto bytes = mesh_test::build(d);
    mesh::MeshView v;
    char why[64];
    CHECK(mesh::validate(bytes.data(), bytes.size(), v, why, sizeof why));
    CHECK(v.header.vertex_count == 6); // one vertex per index
    for (int i = 0; i < 6; ++i) {
        std::int8_t n[4];
        std::memcpy(n, v.vertices + 16 * i + 8, 4);
        CHECK(n[0] == 0 && n[1] == 127 && n[2] == 0); // the quad faces +y (indices wind counter-clockwise from above)
        std::uint16_t index;
        std::memcpy(&index, v.indices + 2 * i, 2);
        CHECK(index == i);
    }
}

NNH_TEST(mesh_build_is_deterministic_and_colours_are_copied) {
    const nnh::Color colors[4] = {{255, 0, 0, 255}, {0, 255, 0, 255}, {0, 0, 255, 128}, {1, 2, 3, 4}};
    nnh::MeshDesc d = mesh_test::quad_desc();
    d.colors = colors;
    const auto a = mesh_test::build(d), b = mesh_test::build(d);
    CHECK(a == b && a.size() == 112 + 48 + 4 * 20 + 12);
    CHECK(a[112 + 48 + 3 * 20 + 16] == 1 && a[112 + 48 + 3 * 20 + 19] == 4);
}

NNH_TEST(mesh_build_refuses_with_reasons) {
    const auto refuses = [](nnh::MeshDesc d, const char* part) {
        const mesh::BuildInfo info = mesh::measure(d);
        return !info.ok && std::strstr(info.error, part) != nullptr;
    };
    nnh::MeshDesc d = mesh_test::quad_desc();
    d.positions = nullptr;
    CHECK(refuses(d, "no positions"));
    d = mesh_test::quad_desc();
    d.vertex_count = 0;
    CHECK(refuses(d, "vertex count"));
    const std::uint16_t bad[3] = {0, 1, 4};
    d = mesh_test::quad_desc();
    d.indices = bad, d.index_count = 3;
    CHECK(refuses(d, "index 2 is 4"));
    d = mesh_test::quad_desc();
    d.index_count = 5;
    CHECK(refuses(d, "index count 5"));
    const Vec3 nan_pos[4] = {{0, 0, 0}, {NAN, 0, 0}, {1, 1, 1}, {0, 1, 0}};
    d = mesh_test::quad_desc();
    d.positions = nan_pos;
    CHECK(refuses(d, "position 1"));
    const nnh::SubmeshDesc subs[1] = {{.first_index = 3, .index_count = 6}};
    d = mesh_test::quad_desc();
    d.submeshes = subs, d.submesh_count = 1;
    CHECK(refuses(d, "submesh 0"));
    nnh::SubmeshDesc shiny[1] = {{.first_index = 0, .index_count = 6}};
    shiny[0].material.shininess = 8;
    d.submeshes = shiny;
    CHECK(refuses(d, "invalid material"));
    d = mesh_test::quad_desc();
    d.indices = nullptr, d.index_count = 0; // 4 vertices, no indices: not whole triangles
    CHECK(refuses(d, "multiple of 3"));
}

NNH_TEST(mesh_build_limits_vertices_and_submeshes) {
    static Vec3 many[65536];
    nnh::MeshDesc d{.positions = many, .vertex_count = 65536};
    CHECK(!mesh::measure(d).ok);
    d.vertex_count = 65535; // no indices: 65 535 = 21 845 triangles, flat normals need 65 535 vertices: fits
    CHECK(mesh::measure(d).ok);
    nnh::SubmeshDesc subs[33];
    for (int i = 0; i < 33; ++i) subs[i] = {.first_index = 0, .index_count = 3};
    d.submeshes = subs, d.submesh_count = 33;
    CHECK(!mesh::measure(d).ok);
    d.submesh_count = 32;
    CHECK(mesh::measure(d).ok);
}

NNH_TEST(mesh_texture_paths_follow_the_rules) {
    CHECK(mesh::valid_texture_path("models/ship") && mesh::valid_texture_path("a-b_c.d/E9"));
    CHECK(!mesh::valid_texture_path("") && !mesh::valid_texture_path("/abs") && !mesh::valid_texture_path("a/../b"));
    CHECK(!mesh::valid_texture_path("sp ace") && !mesh::valid_texture_path("back\\slash"));
    char long_path[70];
    std::memset(long_path, 'a', 64);
    long_path[64] = '\0';
    CHECK(!mesh::valid_texture_path(long_path));
    long_path[63] = '\0';
    CHECK(mesh::valid_texture_path(long_path));
}
