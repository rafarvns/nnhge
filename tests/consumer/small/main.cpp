// nnh_consumer_small: hello's size, in a game project outside the engine (spec 032 AC-9). START quits.

#include <nnh/app.hpp>
#include <nnh/debug.hpp>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>

int main() {
    if (!nnh::init({})) return 1;
    nnh::gfx::set_clear_color(nnh::Screen::Top, nnh::rgb(32, 64, 48));
    while (nnh::frame()) {
        if (nnh::input::pressed(nnh::Button::Start)) break;
        nnh::debug::print(nnh::Screen::Top, 8, 8, "consumer small", nnh::colors::yellow);
        nnh::debug::printf(nnh::Screen::Top, 8, 8 + nnh::debug::line_height(), "frame %lu",
                           static_cast<unsigned long>(nnh::frame_count()));
    }
    nnh::shutdown();
    return 0;
}
