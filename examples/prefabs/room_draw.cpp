#include "room_draw.hpp"

#include <nnh/gfx.hpp>

namespace room {

void draw_room(nnh::Screen screen, Room& room) {
    namespace gfx = nnh::gfx;
    gfx::draw_tilemap(screen, room.map(), {.map_layer = 0, .layer = 0});
    nnh::World& w = room.world();
    w.each<Body>([&](nnh::Entity e, Body& b) {
        nnh::Rect box = b.box;
        if (const Bob* bob = w.get<Bob>(e)) {
            gfx::fill_rect(screen, {box.x + 1, box.y + box.h - 2, box.w - 2, 3}, nnh::rgba(0, 0, 0, 110), {.layer = 1});
            box.y -= 6 + bob->offset;
        }
        gfx::fill_rect(screen, box, b.color, {.layer = 2});
        gfx::draw_rect(screen, box, nnh::rgba(0, 0, 0, 140), {.layer = 3});
    });
}

} // namespace room
