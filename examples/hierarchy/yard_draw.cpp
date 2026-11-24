// Drawing the yard (spec 022 FR-13, FR-15, FR-22): every node with a Look through gfx::draw(screen, texture, world,
// options); the top screen follows the rider with a CameraRig, the bottom one shows the whole yard at zoom 0.5 with the
// HUD (the hierarchy's stats) below it; the debug draw on both when D-Pad down turned it on.

#include "yard.hpp"

#include <cstdio>

namespace yard {

void Yard::draw_nodes(nnh::Screen screen) const {
    namespace gfx = nnh::gfx;
    nnh::World& w = const_cast<nnh::World&>(world_);
    gfx::fill_rect(screen, {0, 224, kWidth, 16}, nnh::rgb(90, 140, 70));
    for (float x : {256.0f, 384.0f}) gfx::fill_rect(screen, {x - 2, 196, 4, 28}, nnh::rgb(110, 80, 50)); // posts
    w.each<nnh::Node, Look>([&](nnh::Entity, nnh::Node& n, Look& l) {
        gfx::draw(screen, art_, n.world(), {.region = l.region, .pivot = l.pivot, .layer = 2});
    });
    for (const Shot& s : shots_)
        if (s.frames > 0) gfx::draw(screen, art_, s.at, {.region = {0, 16, 4, 4}, .pivot = {2, 2}, .layer = 3});
    if (world_.alive(picked_)) {
        const Body* b = w.get<Body>(picked_);
        if (b != nullptr) gfx::draw_rect(screen, b->box, nnh::rgb(255, 255, 0), {.layer = 4});
    }
    if (debug_) tree_.debug_draw(screen);
}

void Yard::draw_hud() const {
    const nnh::HierarchyStats s = tree_.stats();
    char text[160];
    std::snprintf(text, sizeof text,
                  "nodes %d/%d  roots %d  deepest %d\ncomputed %d  refusals %d\n%s\n\nCircle Pad/L/R ship  D-Pad aim\n"
                  "A fire  touch pick  X hold/drop\nB destroy ship  Up detach  Down debug",
                  s.nodes, s.capacity, s.roots, s.deepest, s.computed, s.refusals, held_ ? "holding" : "");
    nnh::gfx::draw_text(nnh::Screen::Bottom, {6, 128}, text,
                        {.color = nnh::rgb(210, 220, 240), .ignore_camera = true});
}

void Yard::draw() const {
    rig_.apply();
    draw_nodes(nnh::Screen::Top);
    nnh::gfx::set_camera(nnh::Screen::Bottom, {{0, 0}, 0.5f});
    draw_nodes(nnh::Screen::Bottom);
    draw_hud();
}

} // namespace yard
