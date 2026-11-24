// The .nnhmesh reader (spec 026 FR-7, T2.4): a valid file passes; every corruption is refused with a reason — magic,
// version, sizes, layout, CRC, counts, stride, non-finite floats, indices out of range, submesh ranges, materials,
// texture indices and paths. Corruptions re-sign the CRC so the deeper check is the one that fires.

#include <cmath>
#include <cstring>
#include <string>

#include "core/crc32.hpp"
#include "mesh_fixture.hpp"

namespace mesh = nnh::detail::mesh;

namespace {

std::vector<std::uint8_t> textured_quad() {
    const char* paths[1] = {"art/checker"};
    const std::uint8_t which[1] = {0};
    return mesh_test::build(mesh_test::quad_desc(), {.texture_paths = paths, .texture_count = 1,
                                                     .submesh_textures = which});
}

void resign(std::vector<std::uint8_t>& b) {
    const std::uint32_t crc = nnh::detail::crc32(b.data() + 112, b.size() - 112);
    std::memcpy(b.data() + 16, &crc, 4);
}

/// The file with `n` bytes at `at` replaced, re-signed; @return the refusal's reason ("" when accepted).
template <typename T>
std::string refusal(T value, std::size_t at, bool sign = true) {
    std::vector<std::uint8_t> b = textured_quad();
    std::memcpy(b.data() + at, &value, sizeof value);
    if (sign) resign(b);
    mesh::MeshView v;
    char why[64] = {};
    return mesh::validate(b.data(), b.size(), v, why, sizeof why) ? std::string() : std::string(why);
}

} // namespace

NNH_TEST(mesh_validate_accepts_a_built_file) {
    const auto b = textured_quad();
    mesh::MeshView v;
    char why[64];
    CHECK(mesh::validate(b.data(), b.size(), v, why, sizeof why));
    CHECK(v.header.texture_count == 1 && std::strcmp(v.textures, "art/checker") == 0);
    CHECK(mesh::submesh(v, 0).material.texture == 0);
}

NNH_TEST(mesh_validate_refuses_each_corruption) {
    const std::size_t sub = 112, tex = 112 + 48, vtx = 112 + 48 + 64, idx = vtx + 4 * 16;
    CHECK(refusal(std::uint8_t{'X'}, 0) == "not a mesh (magic)");
    CHECK(refusal(std::uint32_t{2}, 4) == "unsupported version");
    CHECK(refusal(std::uint32_t{999}, 12) == "wrong sizes");
    CHECK(refusal(std::uint8_t{0x55}, 200, false) == "checksum mismatch");
    CHECK(refusal(std::uint8_t{3}, 20) == "bad flags or stride"); // colour flag without colours: stride 20 ≠ 16
    CHECK(refusal(std::uint8_t{0}, 21) == "submesh count out of range");
    CHECK(refusal(std::uint8_t{9}, 22) == "too many textures");
    CHECK(refusal(std::uint32_t{0}, 24) == "vertex count out of range");
    CHECK(refusal(std::uint32_t{7}, 28) == "index count out of range");
    CHECK(refusal(NAN, 32) == "non-finite bounds");
    CHECK(refusal(-1.0f, 36) == "bad scale or box");
    CHECK(refusal(std::uint32_t{120}, 96) == "wrong layout");
    CHECK(refusal(std::uint16_t{4}, idx + 2) == "index out of range");
    CHECK(refusal(std::uint32_t{3}, sub + 4) == "");                         // first 0, count 3: fine…
    CHECK(refusal(std::uint32_t{9}, sub + 4) == "submesh range out of the indices"); // …9 isn't
    CHECK(refusal(std::uint16_t{4}, sub + 10) == "submesh vertex range");
    CHECK(refusal(INFINITY, sub + 12) == "non-finite uv decode");
    CHECK(refusal(std::uint8_t{8}, sub + 28 + 7) == "invalid material");    // shininess
    CHECK(refusal(std::uint8_t{3}, sub + 28 + 9) == "invalid material");    // alpha mode
    CHECK(refusal(std::uint8_t{1}, sub + 28 + 8) == "texture index out of range");
    CHECK(refusal(std::uint8_t{'.'}, tex + 3) == "");                       // "art.checker" is a fine path
    CHECK(refusal(std::uint16_t{0x2E2E}, tex + 3) == "bad texture path");   // "art..ecker"
    CHECK(refusal(std::uint8_t{' '}, tex) == "bad texture path");
}

NNH_TEST(mesh_validate_refuses_truncated_and_null) {
    auto b = textured_quad();
    mesh::MeshView v;
    char why[64];
    CHECK(!mesh::validate(b.data(), 100, v, why, sizeof why) && std::strcmp(why, "too small") == 0);
    CHECK(!mesh::validate(nullptr, 0, v, why, sizeof why));
    CHECK(!mesh::validate(b.data(), b.size() - 4, v, why, sizeof why) && std::strcmp(why, "wrong sizes") == 0);
}
