// Recording 3D draws (spec 026 FR-17/FR-26/FR-29, T4.1–T4.2): capacity reserved only with 3D on, drops counted,
// misuse asserting and harmless, material overrides clamped, the tint folded into the base colour.

#include "gfx3d_fixture.hpp"
#include "nnh/memory.hpp"

using namespace gfx3d_test;
using nnh::Screen;

namespace nnh {
NNH_MEM_TAG_EXTERN(gfx3d);
}

namespace {
nnh::Mesh g_cube;
}

NNH_TEST(gfx3d_reserves_nothing_with_3d_off) {
    restart(0);
    CHECK(fake::pass3d_log().init_calls == 0);
    CHECK(nnh::gfx3d::stats().max_draws == 0);
    nnh::shutdown();
}

NNH_TEST(gfx3d_reserves_about_120_bytes_per_draw) {
    restart(64);
    const std::size_t bytes = nnh::mem::tag_stats(nnh::nnh_mem_tag_gfx3d).bytes;
    std::printf("    %zu bytes for 64 draws\n", bytes);
    CHECK(bytes >= 64 * 100 && bytes <= 64 * 128);
    CHECK(nnh::gfx3d::stats().max_draws == 64 && fake::pass3d_log().max_draws == 64);
    nnh::shutdown();
    CHECK(nnh::mem::tag_stats(nnh::nnh_mem_tag_gfx3d).bytes == 0);
}

NNH_TEST(gfx3d_draws_past_the_capacity_are_dropped_and_counted) {
    restart(2);
    g_cube = cube();
    next_frame();
    nnh::gfx3d::draw(Screen::Top, g_cube, at({0, 0, 0}));
    nnh::gfx3d::draw(Screen::Top, g_cube, at({0, 0, 0}));
    CHECK(asserts_or_is_ignored([] { nnh::gfx3d::draw(Screen::Top, g_cube, at({0, 0, 0})); }));
    next_frame();
    CHECK(nnh::gfx3d::stats().draws == 2 && nnh::gfx3d::stats().dropped_total == 1);
    g_cube.reset();
    nnh::shutdown();
}

NNH_TEST(gfx3d_misuse_asserts_and_records_nothing) {
    restart();
    g_cube = cube();
    CHECK(asserts_or_is_ignored([] { nnh::gfx3d::draw(Screen::Top, g_cube, at({})); })); // outside a frame
    next_frame();
    CHECK(asserts_or_is_ignored([] { nnh::gfx3d::draw(Screen::Bottom, g_cube, at({})); }));
    CHECK(asserts_or_is_ignored([] { nnh::gfx3d::draw(Screen::Top, nnh::Mesh{}, at({})); }));
    CHECK(asserts_or_is_ignored([] { nnh::gfx3d::draw(Screen::Top, g_cube, at({}), {.submesh = 1}); }));
    CHECK(asserts_or_is_ignored([] {
        nnh::Mat4 m;
        m.m[5] = NAN;
        nnh::gfx3d::draw(Screen::Top, g_cube, m);
    }));
    CHECK(asserts_or_is_ignored([] {
        nnh::Mat4 m;
        m.m[3] = 0.5f; // not affine
        nnh::gfx3d::draw(Screen::Top, g_cube, m);
    }));
    next_frame();
    CHECK(nnh::gfx3d::stats().draws == 0);
    g_cube.reset();
    nnh::shutdown();
    restart(0);
    next_frame();
    CHECK(asserts_or_is_ignored([] {
        auto m = nnh::make_cube(); // 3D off: no mesh either
        CHECK(!m.ok());
        nnh::gfx3d::draw(Screen::Top, nnh::Mesh{}, at({}));
    }));
    nnh::shutdown();
}

NNH_TEST(gfx3d_material_override_and_tint_reach_the_platform) {
    restart();
    g_cube = cube(1, {.base_color = {255, 0, 0, 255}});
    next_frame();
    nnh::Material3D blue{.base_color = {0, 0, 255, 255}};
    nnh::gfx3d::draw(Screen::Top, g_cube, at({0, 0, 0}));
    nnh::gfx3d::draw(Screen::Top, g_cube, at({0, 0, 0}), {.material = &blue, .tint = {255, 255, 255, 128}});
    next_frame();
    const nnh::platform::Draw3D* d[4];
    CHECK(last_draws(d, 4) == 2);
    CHECK(d[0]->material.base[0] == 1.0f && d[0]->material.base[2] == 0.0f);
    CHECK(d[1]->material.base[2] == 1.0f && d[1]->material.base[3] == 128.0f / 255.0f);
    CHECK(asserts_or_is_ignored([] {
        nnh::Material3D bad;
        bad.shininess = 9;
        nnh::gfx3d::draw(Screen::Top, g_cube, at({}), {.material = &bad});
    }));
    g_cube.reset();
    nnh::shutdown();
}
