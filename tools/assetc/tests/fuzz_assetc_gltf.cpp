// Fuzz target body of `nnh-assetc mesh` (spec 026 T3.7, FR-7; see fuzz_assetc.hpp): the input is a .gltf document
// (or a .glb when it starts with "glTF") beside a fixed 120-byte buffer m.bin and a PNG header; an accepted model must
// pass the engine's .nnhmesh validator, a refusal must name a file and a reason.

#include <cstdlib>
#include <cstring>
#include <string>

#include "convert_mesh.hpp"
#include "files.hpp"
#include "fuzz_assetc.hpp"
#include "mesh/mesh_validate.hpp"

namespace nnh::assetc::fuzz {

const char* gltf_seed() {
    return R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],
"nodes":[{"mesh":0,"children":[1],"translation":[1,2,3]},{"mesh":0,"rotation":[0,0.7071,0,0.7071],"scale":[-1,2,1]}],
"meshes":[{"primitives":[{"attributes":{"POSITION":0,"NORMAL":1,"TEXCOORD_0":2},"indices":3,"material":0},
 {"attributes":{"POSITION":0,"COLOR_0":4},"material":1}]}],
"materials":[{"pbrMetallicRoughness":{"baseColorFactor":[1,0.5,0.25,1],"baseColorTexture":{"index":0}},
 "alphaMode":"MASK","alphaCutoff":0.3},{"alphaMode":"BLEND","doubleSided":true,"extensions":{"KHR_materials_unlit":{}}}],
"textures":[{"source":0,"sampler":0}],"images":[{"uri":"t.png"}],"samplers":[{"magFilter":9729,"wrapS":33648}],
"extensionsUsed":["KHR_materials_unlit"],
"buffers":[{"uri":"m.bin","byteLength":120}],
"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":36},
 {"buffer":0,"byteOffset":72,"byteLength":24},{"buffer":0,"byteOffset":96,"byteLength":12},
 {"buffer":0,"byteOffset":108,"byteLength":12}],
"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},
 {"bufferView":1,"componentType":5126,"count":3,"type":"VEC3"},
 {"bufferView":2,"componentType":5126,"count":3,"type":"VEC2"},
 {"bufferView":3,"componentType":5123,"count":6,"type":"SCALAR"},
 {"bufferView":4,"componentType":5121,"normalized":true,"count":3,"type":"VEC4"}]})";
}

namespace {

std::string buffer() {
    std::string b(120, '\0');
    const float pos[9] = {0, 0, 0, 1, 0, 0, 0, 1, 0}, nrm[9] = {0, 0, 1, 0, 0, 1, 0, 0, 1}, uv[6] = {0, 0, 1, 0, 0, 1};
    const std::uint16_t idx[6] = {0, 1, 2, 2, 1, 0};
    const std::uint8_t col[12] = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 128};
    std::memcpy(b.data(), pos, 36);
    std::memcpy(b.data() + 36, nrm, 36);
    std::memcpy(b.data() + 72, uv, 24);
    std::memcpy(b.data() + 96, idx, 12);
    std::memcpy(b.data() + 108, col, 12);
    return b;
}

} // namespace

bool gltf_converts(const char* text, std::size_t size) {
    MemoryFiles files;
    const bool glb = size >= 4 && std::memcmp(text, "glTF", 4) == 0;
    const std::string model = glb ? "/g/assets/m.glb" : "/g/assets/m.gltf";
    files.add(model, std::string(text, size));
    files.add("/g/assets/m.bin", buffer());
    static const char kPng[] = "\x89PNG\r\n\x1a\n\0\0\0\x0DIHDR\0\0\0\x10\0\0\0\x08\x08\x06\0\0\0"; // 16x8
    files.add("/g/assets/t.png", std::string(kPng, sizeof kPng - 1));
    Converted out;
    Problem problem;
    std::vector<std::string> notes;
    if (convert_mesh(files, model, "/g/assets", out, problem, &notes)) {
        nnh::detail::mesh::MeshView view;
        char why[64];
        if (!nnh::detail::mesh::validate(out.bytes.data(), out.bytes.size(), view, why, sizeof why)) std::abort();
        return true;
    }
    if (problem.file.empty() || problem.reason.empty()) std::abort();
    return false;
}

int gltf_target(const std::uint8_t* data, std::size_t size) {
    (void)gltf_converts(reinterpret_cast<const char*>(data), size);
    return 0;
}

} // namespace nnh::assetc::fuzz
