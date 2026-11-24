#pragma once
// Helpers for the particle drawing tests (spec 024 stage 4): the fake platform's items of the last frame, compared
// field by field, and the gfx::draw / fill_rect calls equivalent to an emitter's particles.

#include "../gfx/gfx_fixture.hpp"
#include "nnh/transform.hpp"
#include "particles_fixture.hpp"

namespace particles_test {

namespace fake = nnh::platform_fake;
using nnh::platform::DrawItem2D;

inline bool same_item(const DrawItem2D& a, const DrawItem2D& b) {
    return a.kind == b.kind && a.layer == b.layer && a.flip_x == b.flip_x && a.flip_y == b.flip_y &&
           a.texture == b.texture && a.region.x == b.region.x && a.region.y == b.region.y && a.region.w == b.region.w &&
           a.region.h == b.region.h && bits(a.x) == bits(b.x) && bits(a.y) == bits(b.y) && bits(a.w) == bits(b.w) &&
           bits(a.h) == bits(b.h) && bits(a.pivot_x) == bits(b.pivot_x) && bits(a.pivot_y) == bits(b.pivot_y) &&
           bits(a.rotation) == bits(b.rotation) && a.color.r == b.color.r && a.color.g == b.color.g &&
           a.color.b == b.color.b && a.color.a == b.color.a;
}

/// round(a·b/255) per channel.
inline nnh::Color times(nnh::Color a, nnh::Color b) {
    auto m = [](int x, int y) { return static_cast<std::uint8_t>((x * y + 127) / 255); };
    return {m(a.r, b.r), m(a.g, b.g), m(a.b, b.b), m(a.a, b.a)};
}

/// Copies the last frame's items.
inline int last_items(DrawItem2D* out, int capacity) {
    const int n = fake::state().last_frame_item_count;
    for (int i = 0; i < n && i < capacity; ++i) out[i] = fake::state().last_frame_items[i];
    return n;
}

/// What gfx::draw would record for particle `v` of `e` (textured), or gfx::fill_rect (squares), on the top screen.
/// `regions`/`pivots` are the frames as the test resolved them; `tint` already has the opacity in its alpha.
inline void draw_equivalent(const nnh::ParticleEmitter& e, const nnh::ParticleView& v, const nnh::Texture* texture,
                            const nnh::Region* regions, const nnh::Vec2* pivots, nnh::Color tint, int layer,
                            bool ignore_camera) {
    const bool local = e.desc().space == nnh::ParticleSpace::Local;
    const nnh::Color c = times(v.color, tint);
    if (texture != nullptr) {
        nnh::DrawOptions o{.region = regions[v.frame], .pivot = pivots[v.frame], .rotation = v.rotation,
                           .scale = {v.size, v.size}, .tint = c, .layer = layer, .ignore_camera = ignore_camera};
        if (local) {
            const nnh::WorldTransform2D world = nnh::compose(e.transform(), nnh::Transform2D{.position = v.position});
            nnh::gfx::draw(nnh::Screen::Top, *texture, world, o);
        } else {
            nnh::gfx::draw(nnh::Screen::Top, *texture, v.position, o);
        }
        return;
    }
    nnh::Vec2 p = v.position;
    float sx = v.size, sy = v.size;
    if (local) {
        p = nnh::transform_point(e.transform().map, p);
        const nnh::Vec2 s = e.transform().scale;
        sx = (s.x < 0 ? -s.x : s.x) * v.size;
        sy = (s.y < 0 ? -s.y : s.y) * v.size;
    }
    nnh::gfx::fill_rect(nnh::Screen::Top, {p.x - sx * 0.5f, p.y - sy * 0.5f, sx, sy}, c,
                        {.layer = layer, .ignore_camera = ignore_camera});
}

} // namespace particles_test
