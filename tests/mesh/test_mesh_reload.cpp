// The mesh kind of hot reload (spec 031 T6.1, FR-7): the same Mesh handle shows the reloaded model (another vertex
// count), its texture owned as before, the old record freed (one live platform mesh); a damaged file refuses the batch.

#include <vector>

#include "../reload/reload_fixture.hpp"
#include "mesh_fixture.hpp"
#include "nnh/gfx3d.hpp"
#include "nnh/mesh.hpp"

using reload_test::Put;

namespace {

std::vector<std::uint8_t> g_quad, g_tri;
const char kTexture[] = {'F', 'A', 'K', 'E', 16, 0, 8, 0};
const nnh::Vec3 kTriPositions[3] = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
const std::uint16_t kTriIndices[3] = {0, 1, 2};

void start_models() {
    nnh::shutdown();
    reload_test::fake::reset();
    const char* paths[1] = {"art/checker"};
    const std::uint8_t which[1] = {0};
    g_quad = mesh_test::build(mesh_test::quad_desc(), {paths, 1, which});
    g_tri = mesh_test::build({.positions = kTriPositions, .vertex_count = 3, .indices = kTriIndices, .index_count = 3});
    static const char index[] = "art/checker.png\nmodels/gem.nnhmesh\n";
    reload_test::fake::add_asset(".nnh_assets", index, sizeof index - 1);
    reload_test::fake::add_asset("art/checker.png", kTexture, sizeof kTexture);
    reload_test::fake::add_asset("models/gem.nnhmesh", g_quad.data(), g_quad.size());
    reload_test::fake::remote_connect();
    nnh::AppConfig config;
    config.gfx.max_draws_3d = 16;
    CHECK(nnh::init(config));
    reload_test::send(remote_test::hello());
    CHECK(nnh::frame());
}

} // namespace

NNH_TEST(mesh_reload_the_same_handle_shows_the_new_model) {
    RELOAD_DEV_ONLY();
    start_models();
    {
        nnh::Mesh gem = std::move(nnh::load_mesh("models/gem").value());
        CHECK(gem.vertex_count() == 4);
        const Put batch[] = {Put{"models/gem.nnhmesh", g_tri.data(), g_tri.size()}};
        reload_test::send_batch(1, batch, 1);
        CHECK(nnh::frame());
        CHECK(nnh::assets::reload_count() == 1);
        CHECK(gem.valid() && gem.vertex_count() == 3 && gem.triangle_count() == 1);
        CHECK(reload_test::fake::pass3d_log().live_meshes == 1);
    }
    nnh::shutdown();
}

NNH_TEST(mesh_reload_a_damaged_file_keeps_the_old_model) {
    RELOAD_DEV_ONLY();
    start_models();
    {
        nnh::Mesh gem = std::move(nnh::load_mesh("models/gem").value());
        static std::vector<std::uint8_t> bad;
        bad = g_tri;
        bad[bad.size() / 2] ^= 0x40;
        const Put batch[] = {Put{"models/gem.nnhmesh", bad.data(), bad.size()}};
        reload_test::send_batch(1, batch, 1);
        CHECK(nnh::frame());
        CHECK(nnh::assets::reload_count() == 0);
        CHECK(gem.vertex_count() == 4);
    }
    nnh::shutdown();
}

NNH_TEST(mesh_reload_without_memory_for_the_new_model_refuses) {
    RELOAD_DEV_ONLY();
    start_models();
    {
        nnh::Mesh gem = std::move(nnh::load_mesh("models/gem").value());
        reload_test::fake::set_mesh_create_fails(true); // out of linear memory
        const Put batch[] = {Put{"models/gem.nnhmesh", g_tri.data(), g_tri.size()}};
        reload_test::send_batch(1, batch, 1);
        CHECK(nnh::frame());
        CHECK(nnh::assets::reload_count() == 0);
        CHECK(reload_test::records().refused == 1);
        CHECK(reload_test::records().last_refused.stage == nnh::detail::remote::ReloadStage::Memory);
        CHECK(gem.vertex_count() == 4);
        reload_test::fake::set_mesh_create_fails(false);
    }
    nnh::shutdown();
}

NNH_TEST(mesh_reload_draws_recorded_after_the_swap_use_the_new_model) {
    RELOAD_DEV_ONLY();
    start_models();
    {
        nnh::Mesh gem = std::move(nnh::load_mesh("models/gem").value());
        const Put batch[] = {Put{"models/gem.nnhmesh", g_tri.data(), g_tri.size()}};
        reload_test::send_batch(1, batch, 1);
        CHECK(nnh::frame()); // applied at this frame's start
        nnh::gfx3d::draw(nnh::Screen::Top, gem, nnh::Transform3D{.position = {0, 0, -3}});
        CHECK(nnh::frame());
        CHECK(nnh::gfx3d::stats().triangles[0] == 1); // the new file's one triangle, not the quad's 2
    }
    nnh::shutdown();
}
