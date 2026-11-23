#include "hud.hpp"

#include <cstdio>
#include <nnh/gfx.hpp>

namespace room {
namespace {

void line(int y, const char* text, nnh::Color color = nnh::colors::white) {
    nnh::gfx::draw_text(nnh::Screen::Bottom, {6, static_cast<float>(y)}, text,
                        {.color = color, .shadow = true, .layer = 6, .ignore_camera = true});
}

} // namespace

void draw_hud(Room& room) {
    char text[96];
    const nnh::PrefabStats s = room.prefabs().stats();
    std::snprintf(text, sizeof text, "prefabs %d/%d entries %d/%d bytes %d", s.prefabs, s.capacity, s.entries,
                  s.entry_capacity, s.value_bytes);
    line(4, text);
    std::snprintf(text, sizeof text, "spawns %d misses %d  entities %d/%d", s.spawns, s.misses, room.world().count(),
                  room.world().capacity());
    line(14, text, s.misses > 0 ? nnh::rgb(255, 150, 120) : nnh::colors::white);
    const nnh::MapSpawnSummary& m = room.last_map();
    std::snprintf(text, sizeof text, "map spawned %d skip %d unknown %d miss %d", m.spawned, m.skipped, m.unknown,
                  m.misses);
    line(24, text, nnh::rgb(190, 200, 220));
    std::snprintf(text, sizeof text, "> %s", room.prefabs().name(room.selected()));
    line(190, text, nnh::rgb(250, 220, 120));
    room.describe_selected(text, sizeof text);
    line(200, text, nnh::rgb(190, 200, 220));
    int at = std::snprintf(text, sizeof text, "roster:");
    for (int i = 0; i < room.roster_count() && at < static_cast<int>(sizeof text); ++i)
        at += std::snprintf(text + at, sizeof text - at, " %s", room.prefabs().name(room.roster(i)) + 8); // "enemies/"
    line(210, text, nnh::rgb(160, 200, 160));
    line(226, "touch spawn L/R pick A gold X restart", nnh::rgb(150, 160, 190));
}

} // namespace room
