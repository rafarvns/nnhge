// hello: clears both screens, prints text and shows which buttons are held.
// Press START to exit.

#include <nnh/app.hpp>
#include <nnh/debug.hpp>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>

int main() {
    if (!nnh::init({.title = "Hello NNHGE"})) return 1;

    nnh::gfx::set_clear_color(nnh::Screen::Top, nnh::rgb(32, 48, 96));
    nnh::gfx::set_clear_color(nnh::Screen::Bottom, nnh::rgb(24, 24, 24));

    const int line = nnh::debug::line_height();

    while (nnh::frame()) {
        if (nnh::input::pressed(nnh::Button::Start)) break;

        nnh::debug::print(nnh::Screen::Top, 8, 8, "Hello from NNHGE!", nnh::colors::yellow);
        nnh::debug::printf(nnh::Screen::Top, 8, 8 + line, "frame %lu",
                           static_cast<unsigned long>(nnh::frame_count()));

        nnh::debug::print(nnh::Screen::Bottom, 8, 8, "Press START to exit");
        const bool a = nnh::input::held(nnh::Button::A);
        nnh::debug::print(nnh::Screen::Bottom, 8, 8 + 2 * line, a ? "A: held" : "A: -",
                          a ? nnh::colors::green : nnh::colors::gray);
    }

    nnh::shutdown();
    return 0;
}
