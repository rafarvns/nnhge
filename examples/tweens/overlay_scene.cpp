#include "overlay_scene.hpp"

#include <nnh/input.hpp>

using nnh::Duration;
using nnh::Ease;
using nnh::LoopMode;

void OverlayScene::on_enter() {
    tweens_.from_to(&panel_y_, 240.0f, 60.0f, Duration::from_ms(300), {.ease = Ease::BackOut});
    tweens_.from_to(&bar_, 0.0f, 200.0f, Duration::from_seconds(1),
                    {.ease = Ease::SineInOut, .loops = nnh::kForever, .loop_mode = LoopMode::Yoyo});
    tweens_.from_to(&bar_color_, nnh::rgb(255, 80, 80), nnh::rgb(80, 255, 120), Duration::from_ms(700),
                    {.loops = nnh::kForever, .loop_mode = LoopMode::Yoyo});
}

void OverlayScene::update() {
    if (nnh::input::pressed(nnh::Button::Start)) nnh::scenes::pop();
}

void OverlayScene::draw() {
    const nnh::Screen bottom = nnh::Screen::Bottom;
    nnh::gfx::fill_rect(bottom, {20, panel_y_, 280, 120}, nnh::rgba(10, 10, 20, 230), {.layer = 5});
    nnh::gfx::draw_text(bottom, {160, panel_y_ + 12}, "OVERLAY",
                        {.scale = 2, .align = nnh::TextAlign::Center, .layer = 6});
    nnh::gfx::draw_text(bottom, {160, panel_y_ + 40}, "the showcase below is frozen\nSTART: back",
                        {.align = nnh::TextAlign::Center, .layer = 6});
    nnh::gfx::fill_rect(bottom, {60, panel_y_ + 90, bar_, 10}, bar_color_, {.layer = 6});
}
