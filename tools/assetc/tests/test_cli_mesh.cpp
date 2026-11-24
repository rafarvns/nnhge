// The glTF converter (spec 026 FR-6/FR-7, AC-4, T3.2–T3.6): every fixture of tests/data/gltf converts as expected —
// textures, samplers, baked node transforms (a mirrored node keeps its faces outward), vertex colours, flat normals,
// cut-out/blended/unlit materials merged by material, .glb — the same bytes as create_mesh's builder from the same
// arrays, each bad_<rule>.gltf refused with its expected message, notes for ignored features, the depfile's inputs.

#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

#include "cli.hpp"
#include "convert_mesh.hpp"
#include "mesh/mesh_build.hpp"
#include "mesh/mesh_validate.hpp"
#include "test.hpp"

namespace mesh = nnh::detail::mesh;
using nnh::assetc::Converted;
using nnh::assetc::Problem;

namespace {

const std::string kDir = NNH_GLTF_DIR;

bool convert(const std::string& name, Converted& out, Problem& problem, std::vector<std::string>* notes = nullptr) {
    nnh::assetc::DiskFiles files;
    return nnh::assetc::convert_mesh(files, kDir + "/" + name, kDir, out, problem, notes);
}

mesh::MeshView view_of(const Converted& c) {
    mesh::MeshView v;
    char why[64];
    CHECK(mesh::validate(c.bytes.data(), c.bytes.size(), v, why, sizeof why));
    return v;
}

std::string read_text(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    std::stringstream s;
    s << f.rdbuf();
    std::string t = s.str();
    while (!t.empty() && (t.back() == '\n' || t.back() == '\r')) t.pop_back();
    return t;
}

} // namespace

NNH_TEST(gltf_textured_cube_converts_with_its_texture_and_sampler) {
    Converted c;
    Problem p;
    CHECK(convert("cube_textured.gltf", c, p));
    const mesh::MeshView v = view_of(c);
    CHECK(v.header.vertex_count == 24 && v.header.index_count == 36 && v.header.flags == mesh::kFlagUv);
    CHECK(v.header.texture_count == 1 && std::strcmp(v.textures, "checker") == 0);
    const mesh::MaterialRecord m = mesh::submesh(v, 0).material;
    CHECK(m.texture == 0 && m.filter == 0 && m.wrap_u == 1 && m.wrap_v == 0); // nearest, clamp × repeat
    CHECK(m.base[0] == 255 && m.base[1] == 128 && m.base[2] == 64 && m.base[3] == 255);
    CHECK(c.dependencies.size() == 3); // the model, its .bin, its PNG
}

NNH_TEST(gltf_conversion_gives_the_bytes_of_the_engine_builder) {
    // The same cube as make_fixtures.py, built from arrays the way create_mesh does (AC-4).
    const nnh::Vec3 faces[6][3] = {{{1, 0, 0}, {0, 0, -1}, {0, 1, 0}}, {{-1, 0, 0}, {0, 0, 1}, {0, 1, 0}},
                                   {{0, 1, 0}, {1, 0, 0}, {0, 0, -1}}, {{0, -1, 0}, {1, 0, 0}, {0, 0, 1}},
                                   {{0, 0, 1}, {1, 0, 0}, {0, 1, 0}},  {{0, 0, -1}, {-1, 0, 0}, {0, 1, 0}}};
    std::vector<nnh::Vec3> pos, nrm;
    std::vector<nnh::Vec2> uv;
    std::vector<std::uint16_t> idx;
    const float corners[4][4] = {{-1, -1, 0, 1}, {1, -1, 1, 1}, {1, 1, 1, 0}, {-1, 1, 0, 0}};
    for (const auto& f : faces) {
        const auto base = static_cast<std::uint16_t>(pos.size());
        for (const auto& k : corners) {
            pos.push_back((f[0] + f[1] * k[0] + f[2] * k[1]) * 0.5f);
            nrm.push_back(f[0]);
            uv.push_back({k[2], k[3]});
        }
        for (const int i : {0, 1, 2, 0, 2, 3}) idx.push_back(static_cast<std::uint16_t>(base + i));
    }
    nnh::SubmeshDesc sub{0, 36, {}};
    sub.material.base_color = {255, 128, 64, 255};
    sub.material.filter = nnh::TextureFilter::Nearest;
    sub.material.wrap_u = nnh::TextureWrap::Clamp;
    const nnh::MeshDesc d{.positions = pos.data(), .vertex_count = 24, .normals = nrm.data(), .uvs = uv.data(),
                          .indices = idx.data(), .index_count = 36, .submeshes = &sub, .submesh_count = 1};
    const char* paths[1] = {"checker"};
    const std::uint8_t which[1] = {0};
    const mesh::BuildExtras extras{paths, 1, which};
    const mesh::BuildInfo info = mesh::measure(d, extras);
    std::vector<std::uint8_t> code(info.size);
    CHECK(info.ok && mesh::write(d, extras, code.data(), code.size()));
    Converted c;
    Problem p;
    CHECK(convert("cube_textured.gltf", c, p));
    CHECK(c.bytes == code);
}

NNH_TEST(gltf_nodes_are_baked_and_mirrored_ones_keep_their_faces_out) {
    Converted c;
    Problem p;
    CHECK(convert("multi_node.gltf", c, p));
    const mesh::MeshView v = view_of(c);
    CHECK(v.header.vertex_count == 9 && v.header.index_count == 9);
    // Every triangle's winding agrees with its stored normal (the mirrored node's winding was flipped).
    for (int t = 0; t < 3; ++t) {
        nnh::Vec3 p3[3];
        std::int8_t n[4];
        for (int k = 0; k < 3; ++k) {
            std::uint16_t i;
            std::memcpy(&i, v.indices + 2 * (3 * t + k), 2);
            std::int16_t q[4];
            std::memcpy(q, v.vertices + 12 * i, 8);
            std::memcpy(n, v.vertices + 12 * i + 8, 4);
            p3[k] = {q[0] * v.header.pos_scale[0], q[1] * v.header.pos_scale[1], q[2] * v.header.pos_scale[2]};
        }
        const nnh::Vec3 e1 = p3[1] - p3[0], e2 = p3[2] - p3[0];
        const nnh::Vec3 face{e1.y * e2.z - e1.z * e2.y, e1.z * e2.x - e1.x * e2.z, e1.x * e2.y - e1.y * e2.x};
        CHECK(face.x * n[0] + face.y * n[1] + face.z * n[2] > 0);
    }
    // The child turned by 90° about +y under the parent at x = 2: its x-axis vertex (1, 0, 0) lands at (2, 0, −1);
    // the parent reaches x = 3, the mirrored node z = −3.
    CHECK(std::fabs(v.header.aabb_max[0] - 3.0f) < 1e-3f && std::fabs(v.header.aabb_min[2] + 3.0f) < 1e-3f);
}

NNH_TEST(gltf_vertex_colours_flat_normals_materials_and_glb) {
    Converted c;
    Problem p;
    CHECK(convert("vertex_colors.gltf", c, p));
    mesh::MeshView v = view_of(c);
    CHECK(v.header.flags == mesh::kFlagColor && v.header.stride == 16);
    CHECK(v.vertices[2 * 16 + 12 + 2] == 255 && v.vertices[2 * 16 + 12 + 3] == 128); // the third vertex: blue, ½ alpha
    CHECK(convert("no_normals.gltf", c, p));
    v = view_of(c);
    CHECK(v.header.vertex_count == 6 && v.vertices[8 + 1] == 127); // unwelded, flat normal up
    CHECK(convert("cutout_blend.gltf", c, p));
    v = view_of(c);
    CHECK(v.header.submesh_count == 2);
    const mesh::SubmeshRecord a = mesh::submesh(v, 0), b = mesh::submesh(v, 1);
    CHECK(a.index_count == 6 && a.material.alpha == 1 && a.material.cutoff == 64 && (a.material.flags & 4) == 0);
    CHECK(b.index_count == 3 && b.material.alpha == 2 && (b.material.flags & 1) == 0 && b.material.base[2] == 255);
    CHECK(b.material.base[3] == 128);
    CHECK(convert("cube.glb", c, p));
    v = view_of(c);
    CHECK(v.header.vertex_count == 24 && v.header.flags == 0 && c.dependencies.size() == 1);
}

NNH_TEST(gltf_each_bad_fixture_is_refused_with_its_reason) {
    for (const char* name : {"mode", "sparse", "required_extension", "embedded_image", "data_uri", "buffer_name",
                             "index_range", "accessor_range", "texture_size", "version", "no_scene", "json"}) {
        Converted c;
        Problem p;
        const bool ok = convert(std::string("bad_") + name + ".gltf", c, p);
        const std::string text = nnh::assetc::problem_text(p);
        const std::string expected = read_text(kDir + "/bad_" + name + ".txt");
        const bool has = text.find(expected) != std::string::npos;
        if (ok || !has) std::printf("    bad_%s: got \"%s\", want \"%s\"\n", name, text.c_str(), expected.c_str());
        CHECK(!ok && has && c.bytes.empty());
        CHECK(text.find(std::string("bad_") + name + ".gltf: ") != std::string::npos); // `file: reason`
    }
}

NNH_TEST(gltf_ignored_features_are_noted) {
    nnh::assetc::MemoryFiles files;
    std::string bin(36, '\0');
    const float tri[9] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
    std::memcpy(bin.data(), tri, 36);
    files.add("a/m.bin", bin);
    files.add("a/m.gltf", R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],"nodes":[{"mesh":0}],
        "meshes":[{"primitives":[{"attributes":{"POSITION":0},"targets":[{"POSITION":0}]}]}],
        "animations":[{"channels":[],"samplers":[]}],"skins":[{"joints":[0]}],"extensionsUsed":["KHR_lights_punctual"],
        "buffers":[{"uri":"m.bin","byteLength":36}],"bufferViews":[{"buffer":0,"byteLength":36}],
        "accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"}]})");
    Converted c;
    Problem p;
    std::vector<std::string> notes;
    CHECK(nnh::assetc::convert_mesh(files, "a/m.gltf", "a", c, p, &notes));
    std::string all;
    for (const std::string& n : notes) all += n + "\n";
    CHECK(all.find("skins") != std::string::npos && all.find("animations") != std::string::npos);
    CHECK(all.find("morph targets") != std::string::npos && all.find("KHR_lights_punctual") != std::string::npos);
}

NNH_TEST(gltf_more_than_65535_vertices_is_refused) {
    nnh::assetc::MemoryFiles files;
    const int n = 65538; // a multiple of 3, one triangle list
    std::string bin(static_cast<std::size_t>(n) * 12, '\0');
    for (int i = 0; i < n; ++i) {
        const float p[3] = {static_cast<float>(i % 7), static_cast<float>(i % 5), static_cast<float>(i % 3)};
        std::memcpy(bin.data() + 12 * i, p, 12);
    }
    files.add("a/big.bin", bin);
    const std::string len = std::to_string(bin.size());
    files.add("a/big.gltf", R"({"asset":{"version":"2.0"},"scenes":[{"nodes":[0]}],"nodes":[{"mesh":0}],
        "meshes":[{"primitives":[{"attributes":{"POSITION":0,"NORMAL":0}}]}],
        "buffers":[{"uri":"big.bin","byteLength":)" + len + R"(}],"bufferViews":[{"buffer":0,"byteLength":)" + len +
                                  R"(}],"accessors":[{"bufferView":0,"componentType":5126,"count":65538,"type":"VEC3"}]})");
    Converted c;
    Problem p;
    CHECK(!nnh::assetc::convert_mesh(files, "a/big.gltf", "a", c, p));
    CHECK(p.reason.find("65535 vertices") != std::string::npos);
}

NNH_TEST(gltf_cli_writes_the_mesh_and_its_depfile) {
    const std::string out = std::string(NNH_ASSETC_TEST_TMP) + "/cube_textured.nnhmesh";
    const std::string dep = out + ".d";
    const std::string in = kDir + "/cube_textured.gltf";
    const char* argv[] = {"nnh-assetc", "mesh", in.c_str(), out.c_str(), "--assets", kDir.c_str(), "--depfile", dep.c_str()};
    CHECK(nnh::assetc::run_cli(8, argv, stdout, stderr) == 0);
    const std::string d = read_text(dep);
    CHECK(d.find("cube_textured.bin") != std::string::npos && d.find("checker.png") != std::string::npos);
}
