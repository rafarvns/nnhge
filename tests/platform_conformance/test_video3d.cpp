// Platform contract conformance for 3D (spec 026, nnh/platform/video3d.hpp): every platform with 3D starts it, copies
// a mesh and frees it, and runs an empty pass between 2D draws without failing.

#include "nnh/app.hpp"
#include "nnh/platform/app.hpp"
#include "nnh/platform/video.hpp"
#include "nnh/platform/video3d.hpp"
#include "test.hpp"

NNH_TEST(video3d_starts_copies_a_mesh_and_runs_a_pass) {
    nnh::AppConfig config;
    config.gfx.max_draws_3d = 8;
    CHECK(nnh::platform::init(config));
    CHECK(nnh::platform::video3d_init({.max_draws = 8}));
    const std::int16_t vertices[3 * 6] = {0, 0, 0, 0, 0, 127, 1000, 0, 0, 0, 0, 127, 0, 1000, 0, 0, 0, 127};
    const std::uint16_t indices[3] = {0, 1, 2};
    nnh::platform::MeshUpload up;
    up.vertices = vertices;
    up.vertex_count = 3;
    up.stride = 12;
    up.indices = indices;
    up.index_count = 3;
    nnh::platform::MeshInfo info;
    CHECK(nnh::platform::mesh_create(up, info) && info.handle != 0 && info.bytes >= 3 * 12 + 3 * 2);
    CHECK(nnh::platform::poll_events());
    nnh::platform::begin_frame({0, 0, 0, 255}, {0, 0, 0, 255});
    nnh::platform::Pass3D pass;
    for (int i = 0; i < 16; ++i) pass.projection[i] = (i % 5 == 0) ? 1.0f : 0.0f;
    nnh::platform::begin_3d(nnh::Screen::Top, pass);
    nnh::platform::Draw3D draw;
    draw.mesh = info.handle;
    draw.index_count = 3;
    nnh::platform::draw_3d(draw);
    nnh::platform::end_3d();
    nnh::platform::end_frame();
    nnh::platform::mesh_destroy(info.handle);
    nnh::platform::video3d_shutdown();
    nnh::platform::shutdown();
}
