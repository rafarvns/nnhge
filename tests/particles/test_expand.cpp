// The draw expansion (spec 024 FR-13, FR-14, FR-19; T4.2): an emitter's items equal, field for field, the items of
// the equivalent gfx::draw / gfx::fill_rect calls (DrawList::build's), at zoom 0.5, 1 and 2, ignoring the camera,
// rotated, scaled, tinted, every frame mode, whole texture and pivots, local mirrored emitters; whole-emitter and
// per-particle culling; newest first.

#include <initializer_list>

#include "draw_fixture.hpp"

using nnh::ParticleDesc;
using nnh::ParticleEmitter;
using nnh::Region;
using nnh::Screen;
using nnh::Vec2;
using particles_test::DrawItem2D;

namespace {

constexpr int kMax = 64;

struct Case {
    float zoom = 1;
    Vec2 camera{};
    bool ignore_camera = false;
    nnh::ParticleSpace space = nnh::ParticleSpace::World;
    nnh::ParticleFrameMode mode = nnh::ParticleFrameMode::Random;
    nnh::ParticleOrder order = nnh::ParticleOrder::OldestFirst;
    bool textured = true;
    nnh::ParticleDrawOptions options{.layer = 3, .tint = nnh::rgba(200, 150, 255, 220), .opacity = 0.7f};
};

ParticleDesc gallery_desc(const Case& c) {
    ParticleDesc d;
    d.shape = nnh::ParticleShape::Circle;
    d.radius = 20;
    d.speed = {5, 40};
    d.spread = nnh::kFullTurn;
    d.rotation = {-3, 3};
    d.spin = {-2, 2};
    d.size = {0.5f, 2.5f};
    d.size_keys[0] = {0, 1};
    d.size_keys[1] = {1, 0.25f};
    d.color_min = nnh::rgba(40, 80, 120, 160);
    d.color_max = nnh::rgba(250, 240, 230, 255);
    d.color_keys[0] = {0, nnh::colors::white};
    d.color_keys[1] = {1, nnh::rgba(255, 64, 0, 0)};
    d.lifetime = {nnh::Frames{30}, nnh::Frames{90}};
    d.frames[0].region = {0, 0, 16, 16};
    d.frames[1].region = {16, 0, 8, 12};
    d.frames[1].center_pivot = false;
    d.frames[1].pivot = {2, 9};
    d.frames[2].region = {}; // the whole texture
    d.frame_mode = c.mode;
    d.space = c.space;
    d.order = c.order;
    return d;
}

/// The emitter's items against the equivalent calls' items, one frame each.
void compare(const Case& c) {
    gfx_test::restart({.gfx = {.max_draws = 256, .max_particles = 512}});
    nnh::Texture tex = gfx_test::load("hero"); // 64 x 32
    const Region regions[3] = {{0, 0, 16, 16}, {16, 0, 8, 12}, {0, 0, 64, 32}};
    const Vec2 pivots[3] = {{8, 8}, {2, 9}, {32, 16}};
    ParticleEmitter e = particles_test::make(gallery_desc(c), 128);
    e.set_transform(nnh::Transform2D{.position = {200, 120}, .rotation = 0.4f, .scale = {-1.5f, 1.25f}});
    e.emit(30);
    for (int i = 0; i < 12; ++i) e.update();
    nnh::gfx::set_camera(Screen::Top, {c.camera, c.zoom});
    CHECK(nnh::frame());
    if (c.textured) {
        e.draw(Screen::Top, tex, c.options);
    } else {
        e.draw(Screen::Top, c.options);
    }
    CHECK(nnh::frame());
    DrawItem2D got[kMax];
    const int n = particles_test::last_items(got, kMax);
    nnh::Color tint = c.options.tint;
    tint.a = static_cast<std::uint8_t>(static_cast<float>(tint.a) * c.options.opacity + 0.5f);
    const int alive = e.alive();
    for (int k = 0; k < alive; ++k) {
        const int i = c.order == nnh::ParticleOrder::NewestFirst ? alive - 1 - k : k;
        particles_test::draw_equivalent(e, e.particle(i), c.textured ? &tex : nullptr, regions, pivots, tint,
                                        c.options.layer, c.ignore_camera || c.options.ignore_camera);
    }
    CHECK(nnh::frame());
    DrawItem2D want[kMax];
    const int m = particles_test::last_items(want, kMax);
    CHECK(n > 0 && n == m && n <= kMax);
    for (int i = 0; i < n && i < m && i < kMax; ++i) CHECK(particles_test::same_item(got[i], want[i]));
    CHECK(e.stats().drawn == alive && e.stats().culled == 0); // all visible: the comparison covers every particle
    nnh::shutdown();
}

} // namespace

NNH_TEST(expanded_particles_equal_gfx_draw_items_at_every_zoom) {
    compare({.zoom = 0.5f, .camera = {-37.25f, 11.5f}});
    compare({.zoom = 1, .camera = {37.25f, 11.5f}});
    compare({.zoom = 2, .camera = {140.25f, 70.5f}});
}

NNH_TEST(expanded_particles_equal_gfx_draw_items_ignoring_the_camera) {
    Case c{.zoom = 2, .camera = {100, 50}};
    c.options.ignore_camera = true;
    compare(c);
}

NNH_TEST(expanded_particles_equal_gfx_draw_items_in_every_frame_mode) {
    for (auto mode : {nnh::ParticleFrameMode::First, nnh::ParticleFrameMode::OverLife, nnh::ParticleFrameMode::Rate}) {
        compare({.mode = mode});
    }
}

NNH_TEST(expanded_local_mirrored_particles_equal_transformed_draws) {
    compare({.zoom = 1, .camera = {}, .space = nnh::ParticleSpace::Local});
    compare({.zoom = 2, .camera = {80, 40}, .space = nnh::ParticleSpace::Local});
}

NNH_TEST(expanded_squares_equal_fill_rects) {
    compare({.textured = false});
    compare({.zoom = 0.5f, .camera = {-20, 3}, .space = nnh::ParticleSpace::Local, .textured = false});
}

NNH_TEST(expanded_particles_newest_first) {
    compare({.order = nnh::ParticleOrder::NewestFirst});
}

NNH_TEST(emitter_entirely_off_screen_is_culled_whole) {
    gfx_test::restart({.gfx = {.max_particles = 64}});
    ParticleDesc d;
    d.size = {4, 4};
    ParticleEmitter e = particles_test::make(d, 64);
    e.set_position({-500, 100});
    e.emit(20);
    CHECK(nnh::frame());
    e.draw(Screen::Top);
    CHECK(nnh::frame());
    CHECK(particles_test::fake::state().last_frame_item_count == 0);
    CHECK(e.stats().drawn == 0 && e.stats().culled == 20 && nnh::gfx::stats().particles_last_frame == 0);
    nnh::shutdown();
}

NNH_TEST(particles_are_culled_one_by_one_at_the_reach_boundary) {
    gfx_test::restart({.gfx = {.max_particles = 64}});
    ParticleDesc d;
    d.size = {4, 4}; // squares: reach = 4 + 2 px of rounding
    ParticleEmitter e = particles_test::make(d, 64);
    e.emit_at(Vec2{-6, 100}, 1);     // exactly at the reach: kept
    e.emit_at(Vec2{-6.01f, 100}, 1); // just past it: culled
    e.emit_at(Vec2{406, 100}, 1);    // right edge: kept
    e.emit_at(Vec2{406.5f, 100}, 1); // past it: culled
    e.emit_at(Vec2{200, 246.5f}, 1); // below: culled
    e.emit_at(Vec2{200, 120}, 1);
    CHECK(nnh::frame());
    e.draw(Screen::Top);
    CHECK(nnh::frame());
    CHECK(e.stats().drawn == 3 && e.stats().culled == 3);
    CHECK(particles_test::fake::state().last_frame_item_count == 3);
    nnh::shutdown();
}
