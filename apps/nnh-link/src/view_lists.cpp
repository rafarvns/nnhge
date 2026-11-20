// The servers and games lists, plus the drawing helpers every screen uses (views.hpp).

#include <nnh_link/address.hpp>

#include "views.hpp"

namespace link_app::view {

using nnh::Screen;
using nnh::gfx::draw_text;
using nnh::gfx::fill_rect;
using namespace layout;

namespace {
constexpr nnh::Rect kButton{kButtonX, kButtonY, kButtonW, kButtonH};
} // namespace

void header(const std::string& right) {
    fill_rect(Screen::Top, {0, 0, 400, 26}, nnh::rgb(30, 40, 64));
    draw_text(Screen::Top, {8, 5}, "NNH Link", {.color = kAccent, .scale = 2});
    draw_text(Screen::Top, {392, 9}, fit(right, 30).c_str(), {.color = kDim, .align = nnh::TextAlign::Right});
}

void help(const char* text) { draw_text(Screen::Top, {8, 206}, text, {.color = kDim}); }

void button(const char* label) {
    fill_rect(Screen::Bottom, kButton, nnh::rgb(48, 56, 80));
    nnh::gfx::draw_rect(Screen::Bottom, kButton, kAccent);
    draw_text(Screen::Bottom, {160, kButton.y + 12}, label, {.align = nnh::TextAlign::Center});
}

namespace {

void row(int index, int selected, const std::string& text, const std::string& right, nnh::Color right_color) {
    const int on_screen = index - first_row(selected);
    if (on_screen < 0 || on_screen >= kVisibleRows) return;
    const float y = static_cast<float>(kRowTop + on_screen * kRowHeight);
    if (index == selected) fill_rect(Screen::Bottom, {4, y, 312, kRowHeight - 2}, nnh::rgb(52, 72, 112));
    draw_text(Screen::Bottom, {12, y + 6}, fit(text, 26).c_str());
    draw_text(Screen::Bottom, {308, y + 6}, right.c_str(), {.color = right_color, .align = nnh::TextAlign::Right});
}

} // namespace

void draw_servers(const LinkState& link) {
    header("servers");
    const auto& servers = link.servers();
    draw_text(Screen::Top, {8, 40}, "Pick the PC that runs  nnh serve", {.scale = 1});
    if (servers.empty()) {
        draw_text(Screen::Top, {8, 64}, "Looking on the local network...", {.color = kDim});
        draw_text(Screen::Top, {8, 80}, "Start it in your NNHGE repository:  nnh serve", {.color = kDim});
    } else {
        draw_text(Screen::Top, {8, 64}, (std::to_string(servers.size()) + " found").c_str(), {.color = kDim});
    }
    const auto selected = static_cast<std::size_t>(link.selected());
    if (selected < servers.size()) {
        const ServerRow& r = servers[selected];
        draw_text(Screen::Top, {8, 100}, fit(r.server.name, 25).c_str(), {.scale = 2});
        draw_text(Screen::Top, {8, 124}, nnh::link::to_string(r.server.address).c_str(), {.color = kDim});
        draw_text(Screen::Top, {8, 140}, r.paired ? "Paired: connects without a code" : "New: asks for a pairing code",
                  {.color = r.paired ? kGood : kDim});
    }
    help("A connect   X search again   Y forget pairing\nSTART exit");
    for (std::size_t i = 0; i < servers.size(); ++i) {
        const ServerRow& r = servers[i];
        row(static_cast<int>(i), link.selected(), r.server.name, r.paired ? "paired" : "new", r.paired ? kGood : kDim);
    }
    row(static_cast<int>(servers.size()), link.selected(), "Enter an address...", "", kDim);
}

void draw_games(const LinkState& link) {
    header(link.server().name);
    const auto& games = link.games();
    const char* mode = link.release() ? "Release" : "Development (log on the PC)";
    draw_text(Screen::Top, {8, 40}, "Build and run", {.scale = 1});
    if (games.empty()) {
        draw_text(Screen::Top, {8, 64}, "This repository has no games.", {.color = kWarn});
    } else {
        const auto& game = games[static_cast<std::size_t>(link.selected())];
        draw_text(Screen::Top, {8, 64}, fit(game.title, 25).c_str(), {.scale = 2});
        draw_text(Screen::Top, {8, 88}, game.name.c_str(), {.color = kDim});
    }
    draw_text(Screen::Top, {8, 120}, "Build:", {.color = kDim});
    draw_text(Screen::Top, {64, 120}, mode, {.color = link.release() ? kWarn : kGood});
    help("A build and run   X Development/Release\nB other servers");
    for (std::size_t i = 0; i < games.size(); ++i) {
        row(static_cast<int>(i), link.selected(), games[i].title, "", kDim);
    }
}

} // namespace link_app::view
