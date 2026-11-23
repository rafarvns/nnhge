#include "hud.hpp"

#include <cstdio>
#include <nnh/gfx.hpp>

namespace arena {
namespace {

constexpr nnh::Screen kBottom = nnh::Screen::Bottom;

void line(int y, const char* text, nnh::Color color = nnh::colors::white) {
    nnh::gfx::draw_text(kBottom, {6, static_cast<float>(y)}, text,
                        {.color = color, .shadow = true, .layer = 6, .ignore_camera = true});
}

template <typename T>
void kind_line(nnh::World& w, int y, const char* name) {
    const nnh::KindStats s = w.storage<T>().stats();
    char text[64];
    std::snprintf(text, sizeof text, "%-9s %2d/%-3d peak %2d", name, s.live, s.capacity, s.peak);
    line(y, text, nnh::rgb(190, 200, 220));
}

} // namespace

void draw_hud(Arena& arena, nnh::Tag filter, int found) {
    nnh::World& w = arena.world();
    char text[80];
    const nnh::WorldStats s = w.stats();
    std::snprintf(text, sizeof text, "entities %d/%d peak %d misses %d", s.entities, s.capacity, s.peak,
                  s.spawn_misses + s.add_misses);
    line(4, text);
    kind_line<Body>(w, 16, "body");
    kind_line<Velocity>(w, 26, "velocity");
    kind_line<Explosion>(w, 36, "explosion");
    const nnh::PoolStats p = arena.bullets().stats();
    std::snprintf(text, sizeof text, "bullets %d/%d peak %d misses %d", p.live, p.capacity, p.peak, p.misses);
    line(48, text, p.misses > 0 ? nnh::rgb(255, 150, 120) : nnh::colors::white);
    if (filter != nnh::kNoTag) {
        std::snprintf(text, sizeof text, "filter %s: %d found", tag_name(filter), found);
    } else {
        std::snprintf(text, sizeof text, "filter: none");
    }
    line(60, text, nnh::rgb(250, 230, 140));
    if (arena.missile_status()[0] != '\0') line(72, arena.missile_status(), nnh::rgb(255, 160, 160));
    line(216, "pad move  A fire  B zap  X filter\nL/R freeze  touch destroy  SELECT check", nnh::rgb(150, 160, 190));
}

} // namespace arena
