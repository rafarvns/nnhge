// sprites: textures, sprite-sheet frames, shapes and cameras (spec 004).
// Work in progress: the PC and 3DS backends draw from spec 004 stages 4-5; for now this shows that the
// embedded images load. START exits.

#include <nnh/app.hpp>
#include <nnh/debug.hpp>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>
#include <nnh/texture.hpp>

namespace {

void show_load(int line, const char* name, const nnh::Result<nnh::Texture>& result) {
    if (result) {
        nnh::debug::printf(nnh::Screen::Top, 8, line, "%s: %dx%d", name, result.value().width(),
                           result.value().height());
    } else {
        nnh::debug::printf(nnh::Screen::Top, 8, line, "%s: %s", name, result.error().message);
    }
}

} // namespace

int main() {
    if (!nnh::init({.title = "NNHGE Sprites"})) return 1;
    {
        const auto hero = nnh::load_texture("hero");
        const auto tiles = nnh::load_texture("tiles");
        const int line = nnh::debug::line_height();
        while (nnh::frame()) {
            if (nnh::input::pressed(nnh::Button::Start)) break;
            nnh::debug::print(nnh::Screen::Top, 8, 8, "NNHGE sprites - START exits", nnh::colors::yellow);
            show_load(8 + 2 * line, "hero", hero);
            show_load(8 + 3 * line, "tiles", tiles);
            if (hero) nnh::gfx::draw(nnh::Screen::Top, hero.value(), {100, 120}, {.region = {0, 0, 16, 16}, .scale = {4, 4}});
        }
    }
    nnh::shutdown();
    return 0;
}
