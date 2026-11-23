#include "arena_draw.hpp"

namespace arena {
namespace {

namespace gfx = nnh::gfx;
using nnh::Entity;

enum DrawLayer : int { kFloor = 0, kThings = 2, kShots = 3, kMarks = 4 };

nnh::Color color_of(nnh::World& w, Entity e, const Body& b) {
    if (b.layers == bits::kShip) return nnh::rgb(90, 220, 255);
    if (b.layers == bits::kMissile) return nnh::rgb(250, 250, 250);
    if (b.layers == bits::kCoin) return nnh::rgb(255, 210, 60);
    return w.has_tag(e, kFlying) ? nnh::rgb(255, 140, 60) : nnh::rgb(230, 60, 70);
}

} // namespace

void draw_arena(nnh::Screen screen, Arena& arena, const Entity* highlight, int highlighted) {
    nnh::World& w = arena.world();
    gfx::fill_rect(screen, kBounds, nnh::rgb(24, 28, 44), {.layer = kFloor});
    gfx::draw_rect(screen, {kBounds.x - 1, kBounds.y - 1, kBounds.w + 2, kBounds.h + 2}, nnh::rgb(90, 96, 130),
                   {.layer = kFloor});
    // Every body, in ascending slots of the body storage.
    for (auto [e, b] : w.storage<Body>().items()) {
        gfx::fill_rect(screen, b.box, color_of(w, e, b), {.layer = kThings});
        if (w.has_tag(e, kFrozen))
            gfx::draw_rect(screen, {b.box.x - 2, b.box.y - 2, b.box.w + 4, b.box.h + 4}, nnh::rgb(120, 170, 255),
                           {.layer = kMarks});
    }
    for (int i = 0; i < highlighted; ++i) {
        if (const Body* b = w.get<Body>(highlight[i]))
            gfx::draw_rect(screen, {b->box.x - 3, b->box.y - 3, b->box.w + 6, b->box.h + 6}, nnh::colors::white,
                           {.layer = kMarks});
    }
    w.each<Explosion>([&](Entity, Explosion& x) {
        const float r = static_cast<float>(24 - x.frames);
        gfx::draw_rect(screen, {x.at.x - r, x.at.y - r, 2 * r, 2 * r}, nnh::rgba(255, 200, 80, 60 + 9 * x.frames),
                       {.layer = kShots});
    });
    for (auto [h, b] : arena.bullets().items())
        gfx::fill_rect(screen, {b.position.x - 1, b.position.y - 3, 2, 6}, nnh::rgb(200, 255, 140), {.layer = kShots});
    // The missile's line to its target.
    const Missile* m = w.get<Missile>(arena.missile());
    const Body* from = w.get<Body>(arena.missile());
    const Body* to = m != nullptr ? w.get<Body>(m->target) : nullptr;
    if (from != nullptr && to != nullptr)
        gfx::draw_line(screen, centre(from->box), centre(to->box), nnh::rgba(255, 255, 255, 90), {.layer = kMarks});
}

} // namespace arena
