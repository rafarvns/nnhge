// Meshes from code (spec 026 FR-5/FR-8, T2.5–T2.6): 3D off refuses, the bytes the platform gets are the builder's,
// Linear/meshes accounting, handles (move, reset, stale after shutdown, deferred free inside a frame), texture ids,
// refusals, the platform failing.

#include <cstring>
#include <utility>

#include "mesh_fixture.hpp"
#include "mesh/mesh_internal.hpp"
#include "nnh/app.hpp"
#include "nnh/memory.hpp"
#include "nnh/platform_fake.hpp"

namespace fake = nnh::platform_fake;

namespace {

void restart(int max_draws_3d) {
    nnh::shutdown();
    fake::reset();
    nnh::AppConfig config;
    config.gfx.max_draws_3d = max_draws_3d;
    CHECK(nnh::init(config));
}

std::size_t linear_mesh_bytes() {
    nnh::MemStats s = nnh::mem::tag_stats(nnh::nnh_mem_tag_meshes);
    return s.bytes;
}

nnh::Mesh quad() {
    auto r = nnh::create_mesh(mesh_test::quad_desc());
    CHECK(r.ok());
    return r ? std::move(r.value()) : nnh::Mesh{};
}

} // namespace

NNH_TEST(create_mesh_with_3d_off_is_unsupported) {
    restart(0);
    auto r = nnh::create_mesh(mesh_test::quad_desc());
    CHECK(!r.ok() && r.error().code == nnh::ErrorCode::Unsupported);
    CHECK(!nnh::make_cube().ok());
    CHECK(fake::pass3d_log().init_calls == 0); // nothing set up for 3D
    nnh::shutdown();
}

NNH_TEST(create_mesh_uploads_the_builders_bytes) {
    restart(16);
    CHECK(fake::pass3d_log().initialized && fake::pass3d_log().max_draws == 16);
    nnh::Mesh m = quad();
    CHECK(m.valid() && m.vertex_count() == 4 && m.triangle_count() == 2 && m.submesh_count() == 1);
    CHECK(m.bounding_sphere().radius > 1.41f && m.bounds().max.x > 0.99f);
    const auto file = mesh_test::build(mesh_test::quad_desc());
    CHECK(fake::pass3d_log().live_meshes == 1 && fake::pass3d_log().live_mesh_bytes == 4 * 16 + 6 * 2);
    // The platform got the file's vertex and index sections, byte for byte.
    std::size_t size = 0;
    const std::uint8_t* data = fake::mesh_data(nnh::detail::mesh_get(m)->handle, size);
    CHECK(size == 4 * 16 + 6 * 2 && file.size() == 112 + 48 + size);
    CHECK(data != nullptr && std::memcmp(data, file.data() + 112 + 48, size) == 0);
    nnh::shutdown();
}

NNH_TEST(create_mesh_accounts_linear_memory_and_frees_it) {
    restart(16);
    const std::size_t before = nnh::mem::region_stats(nnh::MemRegion::Linear).bytes;
    {
        nnh::Mesh m = quad();
        CHECK(nnh::mem::region_stats(nnh::MemRegion::Linear).bytes == before + 4 * 16 + 6 * 2);
        CHECK(linear_mesh_bytes() > 0);
    }
    CHECK(nnh::mem::region_stats(nnh::MemRegion::Linear).bytes == before);
    CHECK(fake::pass3d_log().live_meshes == 0);
    nnh::shutdown();
}

NNH_TEST(mesh_handles_move_and_reset) {
    restart(16);
    nnh::Mesh a = quad();
    nnh::Mesh b = std::move(a);
    CHECK(!a.valid() && b.valid());
    b.reset();
    CHECK(!b.valid() && fake::pass3d_log().live_meshes == 0);
    nnh::Mesh c = quad();
    nnh::shutdown(); // destroys every mesh; the handle goes stale
    CHECK(!c.valid() && fake::pass3d_log().live_meshes == 0);
    c.reset(); // harmless
}

NNH_TEST(a_mesh_released_inside_a_frame_is_freed_after_it) {
    restart(16);
    nnh::Mesh m = quad();
    CHECK(nnh::frame());
    m.reset();
    CHECK(fake::pass3d_log().live_meshes == 1); // a draw recorded this frame may still use it
    CHECK(nnh::frame());
    CHECK(fake::pass3d_log().live_meshes == 0);
    nnh::shutdown();
}

NNH_TEST(create_mesh_keeps_the_games_materials) {
    restart(16);
    nnh::SubmeshDesc subs[2] = {{.first_index = 0, .index_count = 3}, {.first_index = 3, .index_count = 3}};
    subs[1].material.base_color = {10, 20, 30, 40};
    subs[1].material.texture = {5, 77};
    subs[1].material.alpha = nnh::AlphaMode::Blend;
    nnh::MeshDesc d = mesh_test::quad_desc();
    d.submeshes = subs, d.submesh_count = 2;
    auto r = nnh::create_mesh(d);
    CHECK(r.ok() && r.value().submesh_count() == 2);
    const nnh::Material3D m = r.value().material(1);
    CHECK(m.base_color.b == 30 && m.texture == (nnh::TextureId{5, 77}) && m.alpha == nnh::AlphaMode::Blend);
    CHECK(r.value().material(2).alpha == nnh::AlphaMode::Opaque); // out of range: the default
    nnh::shutdown();
}

NNH_TEST(create_mesh_refuses_bad_data_and_platform_failures) {
    restart(16);
    nnh::MeshDesc d = mesh_test::quad_desc();
    d.vertex_count = 0;
    auto r = nnh::create_mesh(d);
    CHECK(!r.ok() && r.error().code == nnh::ErrorCode::InvalidArgument);
    CHECK(std::strstr(r.error().message, "vertex count") != nullptr);
    fake::set_mesh_create_fails(true);
    auto f = nnh::create_mesh(mesh_test::quad_desc());
    CHECK(!f.ok() && f.error().code == nnh::ErrorCode::OutOfMemory);
    fake::set_mesh_create_fails(false);
    CHECK(nnh::mem::tag_stats(nnh::nnh_mem_tag_meshes).bytes > 0); // only the table remains
    nnh::shutdown();
}

NNH_TEST(init_fails_when_the_platform_has_no_room_for_3d) {
    nnh::shutdown();
    fake::reset();
    fake::set_video3d_init_fails(true);
    nnh::AppConfig config;
    config.gfx.max_draws_3d = 8;
    CHECK(!nnh::init(config));
    fake::reset();
}

NNH_TEST(texture_id_names_a_loaded_texture) {
    CHECK(nnh::texture_id(nnh::Texture{}) == nnh::TextureId{});
}
