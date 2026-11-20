// Connecting, building and the messages (views.hpp).

#include <cstdio>

#include <nnh/app.hpp>

#include "views.hpp"

namespace link_app::view {

using nnh::Screen;
using nnh::gfx::draw_text;
using nnh::gfx::fill_rect;

namespace {

/// The last lines of the build output, wrapped, newest at the bottom, from y down to y_end.
void output(const std::vector<std::string>& lines, float y, float y_end, nnh::Color color) {
    const std::vector<std::string> rows = wrap(lines, 48);
    const std::size_t room = static_cast<std::size_t>((y_end - y) / nnh::kFontLineHeight);
    const std::size_t first = rows.size() > room ? rows.size() - room : 0;
    for (std::size_t i = first; i < rows.size(); ++i, y += nnh::kFontLineHeight) {
        draw_text(Screen::Top, {8, y}, rows[i].c_str(), {.color = color});
    }
}

} // namespace

void draw_progress(const LinkState& link) {
    header(link.server().name);
    if (link.screen() == link_app::Screen::Connecting) {
        draw_text(Screen::Top, {8, 40}, ("Connecting to " + fit(link.server().name, 30) + "...").c_str());
        draw_text(Screen::Top, {8, 60}, nnh::link::to_string(link.server().address).c_str(), {.color = kDim});
        help("B cancel");
        button("Cancel");
        return;
    }
    draw_text(Screen::Top, {8, 34}, fit(link.game().title, 25).c_str(), {.scale = 2});
    const bool receiving = link.build_state() == Client::Build::Receiving;
    if (receiving && link.file_size() > 0) {
        const double done = static_cast<double>(link.received()) / static_cast<double>(link.file_size());
        char text[64];
        std::snprintf(text, sizeof text, "Receiving %llu KB  %d%%", static_cast<unsigned long long>(link.file_size() / 1024),
                      static_cast<int>(done * 100));
        draw_text(Screen::Top, {8, 60}, text, {.color = kAccent});
        fill_rect(Screen::Bottom, {20, 90, 280, 16}, nnh::rgb(12, 14, 22));
        fill_rect(Screen::Bottom, {20, 90, static_cast<float>(280 * done), 16}, kAccent);
    } else {
        draw_text(Screen::Top, {8, 60}, link.release() ? "Building (Release) on the PC..." : "Building on the PC...",
                  {.color = kAccent});
        // A moving bar: the build has no known length.
        const float x = static_cast<float>(nnh::frame_count() * 3 % 240);
        fill_rect(Screen::Bottom, {20, 90, 280, 16}, nnh::rgb(12, 14, 22));
        fill_rect(Screen::Bottom, {20 + x, 90, 40, 16}, kAccent);
    }
    output(link.lines(), 80, 200, kDim);
    help("B cancel");
    button("Cancel");
}

void draw_message(const LinkState& link) {
    header(link.server().name);
    if (link.screen() == link_app::Screen::Done) {
        draw_text(Screen::Top, {8, 40}, "Received", {.color = kGood, .scale = 2});
        draw_text(Screen::Top, {8, 66}, fit(link.message(), 48).c_str());
        draw_text(Screen::Top, {8, 86}, "This system can't start it from here: open it", {.color = kDim});
        draw_text(Screen::Top, {8, 98}, "from the Homebrew Launcher (on the PC:", {.color = kDim});
        draw_text(Screen::Top, {8, 110}, "nnh run <game> --pc).", {.color = kDim});
        help("A back");
        button("Back");
        return;
    }
    draw_text(Screen::Top, {8, 36}, fit(link.message(), 48).c_str(), {.color = kWarn});
    output(link.lines(), 56, 200, nnh::colors::white);
    help("A back");
    button("Back");
}

} // namespace link_app::view
