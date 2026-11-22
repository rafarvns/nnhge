#include "check_scene.hpp"

#include <cstdio>
#include <initializer_list>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>
#include <nnh/log.hpp>

NNH_LOG_CHANNEL(datademo);

void CheckScene::on_enter() {
    result_ = run_data_check();
    format_data_check(result_, line_, sizeof line_);
    if (result_.pass) {
        NNH_LOG_INFO(datademo, "%s", line_);
    } else {
        NNH_LOG_ERROR(datademo, "%s", line_);
    }
}

void CheckScene::update() {
    if (nnh::input::pressed(nnh::Button::B) || nnh::input::pressed(nnh::Button::Start)) nnh::scenes::pop();
}

void CheckScene::draw() {
    const nnh::Color color = result_.pass ? nnh::rgb(60, 200, 90) : nnh::rgb(220, 60, 60);
    const char* verdict = result_.pass ? "PASS" : "FAIL";
    for (nnh::Screen screen : {nnh::Screen::Top, nnh::Screen::Bottom}) {
        const float middle = static_cast<float>(nnh::screen_width(screen)) / 2;
        nnh::gfx::fill_rect(screen, {0, 70, middle * 2, 70}, color);
        nnh::gfx::draw_text(screen, {middle, 84}, verdict, {.scale = 6, .align = nnh::TextAlign::Center});
    }
    char detail[64];
    if (result_.pass) {
        std::snprintf(detail, sizeof detail, "checksum 0x%08X", static_cast<unsigned>(result_.checksum));
    } else {
        std::snprintf(detail, sizeof detail, "%s", result_.failure);
    }
    nnh::gfx::draw_text(nnh::Screen::Top, {200, 20}, "DATA SELF-CHECK", {.scale = 2, .align = nnh::TextAlign::Center});
    nnh::gfx::draw_text(nnh::Screen::Top, {200, 170}, detail, {.scale = 2, .align = nnh::TextAlign::Center});
    char counts[64];
    std::snprintf(counts, sizeof counts, "%d enemies, %d items, %d settings", result_.enemies, result_.items,
                  result_.settings);
    nnh::gfx::draw_text(nnh::Screen::Bottom, {160, 160}, counts, {.align = nnh::TextAlign::Center});
    std::snprintf(counts, sizeof counts, "data index: %u bytes", static_cast<unsigned>(result_.index_bytes));
    nnh::gfx::draw_text(nnh::Screen::Bottom, {160, 176}, counts, {.align = nnh::TextAlign::Center});
    nnh::gfx::draw_text(nnh::Screen::Bottom, {160, 220}, "B back", {.align = nnh::TextAlign::Center});
}
