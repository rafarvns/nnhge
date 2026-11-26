// The showcase (see live_scene.hpp).

#include "live_scene.hpp"

#include <cstdlib>
#include <cstring>

#include <nnh/debug.hpp>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>
#include <nnh/log.hpp>

#include "bench.hpp"
#include "self_check.hpp"

NNH_LOG_CHANNEL_EXTERN(example);

namespace {

const char* shot() {
    const char* s = std::getenv("LIVE_SHOT");
    return s != nullptr ? s : "";
}

void draw_shape(Shape shape, nnh::Vec2 at, nnh::Color c) {
    const nnh::Rect r{at.x - 5, at.y - 10, 10, 10};
    switch (shape) {
    case Shape::Circle: // a round-ish ball: a cross of two rectangles
        nnh::gfx::fill_rect(nnh::Screen::Top, {at.x - 3, at.y - 10, 6, 10}, c);
        nnh::gfx::fill_rect(nnh::Screen::Top, {at.x - 5, at.y - 8, 10, 6}, c);
        break;
    case Shape::Square: nnh::gfx::fill_rect(nnh::Screen::Top, r, c); break;
    case Shape::Diamond:
        nnh::gfx::fill_rect(nnh::Screen::Top, {at.x - 3, at.y - 8, 6, 6}, c);
        nnh::gfx::fill_rect(nnh::Screen::Top, {at.x - 5, at.y - 6, 10, 2}, c);
        break;
    }
}

} // namespace

void LiveScene::respawn(void* self) {
    auto* s = static_cast<LiveScene*>(self);
    s->world_ = World{};
    sim_respawn(s->world_, s->tuning_);
    NNH_LOG_INFO(example, "live: respawn");
}

void LiveScene::on_enter() {
    sim_respawn(world_, tuning_);
    register_live(handles_, tuning_, world_, &state_text_, &frames_, respawn, this);
    NNH_LOG_INFO(example, "live: showcase");
    const char* s = shot();
    if (std::strcmp(s, "stats") == 0) nnh::live::show_panel(nnh::live::Page::Stats);
    if (std::strcmp(s, "tunables") == 0) {
        tuning_.jump = 6.25f; // one value changed
        nnh::live::show_panel(nnh::live::Page::Tunables, "player/jump");
    }
    if (std::strcmp(s, "colour") == 0) nnh::live::show_panel(nnh::live::Page::Tunables, "colors/sky");
    if (std::strcmp(s, "watches") == 0) nnh::live::show_panel(nnh::live::Page::Watches, "player/pos");
    const char* demo = std::getenv("LIVE_DEMO");
    if (demo != nullptr && std::strcmp(demo, "bench") == 0) bench_.start();
    if (std::strcmp(s, "paused") == 0) nnh::live::show_panel(nnh::live::Page::Tunables, "player/jump");
}

void LiveScene::on_exit() { handles_ = LiveHandles{}; }

void LiveScene::update() {
    ++frames_;
    if (std::strcmp(shot(), "paused") == 0 && ++shot_frames_ == 20) nnh::live::pause();
    using nnh::Button;
    const int dir = (nnh::input::held(Button::Right) ? 1 : 0) - (nnh::input::held(Button::Left) ? 1 : 0);
    const bool jump = nnh::input::pressed(Button::A);
    sim_step(world_, tuning_, dir, jump);
    state_text_ = state_name(world_.state);
    if (jump) NNH_LOG_INFO(example, "live: jump %s", state_text_);
    nnh::live::show("spawn/alive", world_.ball_count);
    if (handles_.count.changed()) NNH_LOG_INFO(example, "live: spawn count %d", tuning_.count);
    if (nnh::input::pressed(Button::Y)) self_check();
    if (nnh::input::pressed(Button::X) && !bench_.running()) bench_.start();
    if (bench_.running()) bench_.update();
    if (nnh::input::pressed(Button::Start)) nnh::scenes::pop();
}

void LiveScene::draw() {
    using nnh::Screen;
    nnh::gfx::fill_rect(Screen::Top, {0, 0, 400, 120}, tuning_.sky);
    nnh::Color low = tuning_.sky;
    low.r = static_cast<std::uint8_t>(low.r / 2);
    low.g = static_cast<std::uint8_t>(low.g / 2);
    low.b = static_cast<std::uint8_t>(low.b / 2);
    nnh::gfx::fill_rect(Screen::Top, {0, 120, 400, 80}, low);
    nnh::gfx::fill_rect(Screen::Top, {0, kGroundY, 400, 40}, tuning_.ground);
    for (int i = 0; i < world_.ball_count; ++i) draw_shape(tuning_.shape, world_.balls[i].pos, nnh::rgb(250, 240, 220));
    const nnh::Rect body{world_.pos.x - 8, world_.pos.y - 16, 16, 16};
    nnh::gfx::fill_rect(Screen::Top, body, nnh::rgb(240, 90, 60));
    nnh::gfx::fill_rect(Screen::Top, {body.x + 9, body.y + 4, 3, 3}, nnh::colors::white);
    nnh::gfx::fill_rect(Screen::Top, {body.x + 4, body.y + 4, 3, 3}, nnh::colors::white);
    if (tuning_.hitboxes) nnh::gfx::draw_rect(Screen::Top, body, nnh::colors::yellow);
    nnh::debug::printf(Screen::Top, 4, 4, "live values: SELECT+L overlay, SELECT+R pages");
    nnh::debug::printf(Screen::Top, 4, 14, "%s  balls %d", state_text_, world_.ball_count);
}

void LiveScene::self_check() {
    const std::uint32_t sum = live_self_check(tuning_);
    NNH_LOG_INFO(example, "live: sum %08X", static_cast<unsigned>(sum));
    if (sum == kLiveDefaultsGolden) NNH_LOG_INFO(example, "live: PASS defaults");
}
