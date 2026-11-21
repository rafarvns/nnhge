// input: every 3DS control on screen — buttons, Circle Pad, C-Stick and the touch screen.
// SELECT turns the Circle Pad → D-Pad mirror on and off; START exits.
//
// PC: keyboard (buttons, I/J/K/L Circle Pad, numpad 8/4/2/6 C-Stick), mouse on the bottom screen,
// or a gamepad. Headless screenshot with scripted input (Debug/Development builds):
//   NNH_SIM_INPUT="circle=0.7,0.7;cstick=-1,0;touch=200,150;buttons=A+R" NNH_EXIT_AFTER_FRAMES=30 \
//   NNH_SCREENSHOT=build/input.png build/pc-debug/examples/input/input.exe
// Presses, releases and touches are logged; tests/remote.nnhs drives it from the PC (spec 013):
//   nnh run input --pc --script examples/input/tests/remote.nnhs

#include <cstring>

#include <nnh/app.hpp>
#include <nnh/debug.hpp>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>
#include <nnh/system.hpp>

#include "press_log.hpp"
#include "stick_view.hpp"
#include "touch_log.hpp"

namespace {

/// Button names in two rows; held ones in green.
void draw_buttons(int y) {
    int x = 8;
    int row = 0;
    for (const NamedButton& b : kButtons) {
        if (b.button == nnh::Button::Start) {
            x = 8;
            row = 1;
        }
        const nnh::Color color = nnh::input::held(b.button) ? nnh::colors::green : nnh::colors::gray;
        nnh::debug::print(nnh::Screen::Top, x, y + row * nnh::debug::line_height(), b.name, color);
        x += 8 * (static_cast<int>(std::strlen(b.name)) + 2);
    }
}

} // namespace

int main() {
    if (!nnh::init({.title = "NNHGE Input"})) return 1;
    nnh::gfx::set_clear_color(nnh::Screen::Top, nnh::rgb(24, 32, 56));
    nnh::gfx::set_clear_color(nnh::Screen::Bottom, nnh::rgb(24, 24, 24));
    const int line = nnh::debug::line_height();
    const bool has_c_stick = nnh::system::has(nnh::Capability::CStick);
    {
        TouchLog touch_log; // subscriptions end before shutdown()
        PressLog press_log; // "pressed A" / "released A" in the log, for test scripts
        while (nnh::frame()) {
            if (nnh::input::pressed(nnh::Button::Start)) break;
            if (nnh::input::pressed(nnh::Button::Select)) {
                nnh::input::set_circle_pad_mirrors_dpad(!nnh::input::circle_pad_mirrors_dpad());
            }

            nnh::debug::print(nnh::Screen::Top, 8, 8, "NNHGE input - START exits", nnh::colors::yellow);
            draw_buttons(8 + 2 * line);
            draw_stick(nnh::Screen::Top, 110, 150, "Circle Pad", nnh::input::circle_pad());
            draw_stick(nnh::Screen::Top, 290, 150, "C-Stick", nnh::input::c_stick(), has_c_stick);

            nnh::debug::print(nnh::Screen::Bottom, 8, 8, "Touch this screen");
            touch_log.draw();
            nnh::debug::printf(nnh::Screen::Bottom, 8, 240 - 2 * line - 8, "SELECT: Circle Pad as D-Pad %s",
                               nnh::input::circle_pad_mirrors_dpad() ? "ON" : "off");
        }
    }
    nnh::shutdown();
    return 0;
}
