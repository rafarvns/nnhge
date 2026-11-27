#include "check_scene.hpp"

#include <cstdio>
#include <initializer_list>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>
#include <nnh/log.hpp>

NNH_LOG_CHANNEL(consumer);

void CheckScene::on_enter() { check(); }

void CheckScene::update() {
    if (nnh::input::pressed(nnh::Button::Select)) check(); // the test script's trigger: the line after it attached
    if (nnh::input::pressed(nnh::Button::Start)) nnh::scenes::pop();
}

void CheckScene::check() {
    result_ = run_consumer_check();
    format_consumer_check(result_, line_, sizeof line_);
    // Also on stdout: Release builds compile log info lines out, and the consumer ctest runs every flavour.
    std::printf("%s\n", line_);
    std::fflush(stdout);
    if (result_.pass) {
        NNH_LOG_INFO(consumer, "%s", line_);
    } else {
        NNH_LOG_ERROR(consumer, "%s", line_);
    }
}

void CheckScene::draw() {
    const nnh::Color color = result_.pass ? nnh::rgb(60, 200, 90) : nnh::rgb(220, 60, 60);
    for (nnh::Screen screen : {nnh::Screen::Top, nnh::Screen::Bottom}) {
        const float width = static_cast<float>(nnh::screen_width(screen));
        nnh::gfx::fill_rect(screen, {0, 70, width, 70}, color);
        nnh::gfx::draw_text(screen, {width / 2, 84}, result_.pass ? "PASS" : "FAIL",
                            {.scale = 6, .align = nnh::TextAlign::Center});
    }
    nnh::gfx::draw_text(nnh::Screen::Top, {200, 20}, "CONSUMER SELF-CHECK", {.scale = 2, .align = nnh::TextAlign::Center});
    nnh::gfx::draw_text(nnh::Screen::Top, {200, 170}, line_, {.align = nnh::TextAlign::Center});
    nnh::gfx::draw_text(nnh::Screen::Bottom, {160, 220}, "START quit", {.align = nnh::TextAlign::Center});
}
