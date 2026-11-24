// Mini-fuzz of the .nnhmesh reader (spec 026 T2.4, FR-7): a built textured, coloured quad mutated 20 000 times with
// the size and CRC fixed, so mutations reach the deep checks; the seed itself must be accepted.

#include <cstring>

#include "../mesh/mesh_fixture.hpp"
#include "fuzz_targets.hpp"

NNH_TEST(mesh_reader_target_survives_mutated_meshes) {
    const nnh::Color colors[4] = {{255, 0, 0, 255}, {0, 255, 0, 255}, {0, 0, 255, 128}, {1, 2, 3, 4}};
    nnh::MeshDesc d = mesh_test::quad_desc();
    d.colors = colors;
    const char* paths[2] = {"a/b", "c"};
    const std::uint8_t which[1] = {1};
    const auto seed = mesh_test::build(d, {.texture_paths = paths, .texture_count = 2, .submesh_textures = which});
    CHECK(nnh::fuzz::mesh_fixed_accepted(seed.data(), seed.size()));
    std::uint32_t state = 2026;
    int accepted = 0;
    std::vector<std::uint8_t> buf;
    for (int i = 0; i < 20000; ++i) {
        buf = seed;
        const int flips = 1 + static_cast<int>((state = state * 1664525u + 1013904223u) >> 30);
        for (int f = 0; f < flips; ++f) {
            state = state * 1664525u + 1013904223u;
            buf[(state >> 8) % buf.size()] ^= static_cast<std::uint8_t>(1u << (state & 7u));
        }
        if ((state & 0x300u) == 0) buf.resize(buf.size() - (state >> 12) % 16); // truncations
        nnh::fuzz::mesh_reader_target(buf.data(), buf.size());
        accepted += nnh::fuzz::mesh_fixed_accepted(buf.data(), buf.size()) ? 1 : 0;
    }
    std::printf("    %d of 20000 mutated meshes accepted\n", accepted);
    CHECK(accepted > 0);
}
