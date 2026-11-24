// Loading converted models (spec 026 FR-8, T3.8): a .nnhmesh asset with a texture loads, its texture is owned by the
// mesh (freed with it) and named by the submesh's material; corrupt files, missing files, missing textures and 3D off
// are refused without leaks.

#include <cstring>
#include <vector>

#include "mesh_fixture.hpp"
#include "nnh/app.hpp"
#include "nnh/platform_fake.hpp"

namespace fake = nnh::platform_fake;

namespace {

std::vector<std::uint8_t> g_good, g_bad, g_notex;
const char kTexture[] = {'F', 'A', 'K', 'E', 16, 0, 8, 0}; // 16x8 (the fake image format)

void start(int max_draws_3d = 16) {
    nnh::shutdown();
    fake::reset();
    const char* paths[1] = {"art/checker"};
    const std::uint8_t which[1] = {0};
    g_good = mesh_test::build(mesh_test::quad_desc(), {paths, 1, which});
    g_bad = g_good;
    g_bad[200] ^= 0x40; // CRC mismatch
    const char* missing[1] = {"art/missing"};
    g_notex = mesh_test::build(mesh_test::quad_desc(), {missing, 1, which});
    static const char index[] =
        "art/checker.png\nmodels/bad.nnhmesh\nmodels/notex.nnhmesh\nmodels/quad.nnhmesh\n";
    fake::add_asset(".nnh_assets", index, sizeof index - 1);
    fake::add_asset("art/checker.png", kTexture, sizeof kTexture);
    fake::add_asset("models/quad.nnhmesh", g_good.data(), g_good.size());
    fake::add_asset("models/bad.nnhmesh", g_bad.data(), g_bad.size());
    fake::add_asset("models/notex.nnhmesh", g_notex.data(), g_notex.size());
    nnh::AppConfig config;
    config.gfx.max_draws_3d = max_draws_3d;
    CHECK(nnh::init(config));
}

} // namespace

NNH_TEST(load_mesh_loads_a_model_and_its_texture) {
    start();
    const int textures_before = fake::state().live_textures;
    {
        auto r = nnh::load_mesh("models/quad");
        CHECK(r.ok());
        const nnh::Mesh& m = r.value();
        CHECK(m.valid() && m.vertex_count() == 4 && m.triangle_count() == 2);
        CHECK(fake::state().live_textures == textures_before + 1 && fake::pass3d_log().live_meshes == 1);
        CHECK(m.material(0).texture.slot >= 0); // the loaded texture, owned by the mesh
    }
    CHECK(fake::state().live_textures == textures_before && fake::pass3d_log().live_meshes == 0);
    nnh::shutdown();
}

NNH_TEST(load_mesh_refuses_corrupt_and_missing_files) {
    start();
    auto bad = nnh::load_mesh("models/bad");
    CHECK(!bad.ok() && bad.error().code == nnh::ErrorCode::Corrupted);
    CHECK(std::strstr(bad.error().message, "checksum") != nullptr);
    auto none = nnh::load_mesh("models/none");
    CHECK(!none.ok() && none.error().code == nnh::ErrorCode::NotFound);
    auto notex = nnh::load_mesh("models/notex");
    CHECK(!notex.ok() && fake::pass3d_log().live_meshes == 0); // the uploaded mesh was freed again
    nnh::shutdown();
}

NNH_TEST(load_mesh_with_3d_off_is_unsupported) {
    start(0);
    auto r = nnh::load_mesh("models/quad");
    CHECK(!r.ok() && r.error().code == nnh::ErrorCode::Unsupported);
    nnh::shutdown();
}
