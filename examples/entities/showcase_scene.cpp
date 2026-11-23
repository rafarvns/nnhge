#include "showcase_scene.hpp"

#include "arena_draw.hpp"
#include "hud.hpp"

#include <nnh/gfx.hpp>
#include <nnh/input.hpp>

namespace {

constexpr nnh::Vec2 kBottomCamera{40, 0};
constexpr nnh::Tag kFilters[] = {nnh::Tag::None, arena::kEnemy, arena::kFlying, arena::kCoin};

nnh::Vec2 stick() {
    const nnh::Stick pad = nnh::input::circle_pad();
    nnh::Vec2 d{pad.x, -pad.y};
    if (nnh::input::held(nnh::Button::Left)) d.x = -1;
    if (nnh::input::held(nnh::Button::Right)) d.x = 1;
    if (nnh::input::held(nnh::Button::Up)) d.y = -1;
    if (nnh::input::held(nnh::Button::Down)) d.y = 1;
    return d;
}

} // namespace

void ShowcaseScene::on_enter() {
    arena_.populate();
    filter_ = 0;
    found_count_ = 0;
    nnh::gfx::set_camera(nnh::Screen::Top, {});
    nnh::gfx::set_camera(nnh::Screen::Bottom, {kBottomCamera});
    NNH_LOG_INFO(example, "entities: showcase, %d entities", arena_.world().count());
}

void ShowcaseScene::refresh_filter() {
    const nnh::Tag t = kFilters[filter_];
    found_count_ = t == nnh::Tag::None ? 0 : arena_.find(t, found_, arena::kMaxEntities);
}

void ShowcaseScene::cycle_filter() {
    filter_ = (filter_ + 1) % 4;
    refresh_filter();
    NNH_LOG_INFO(example, "filter: %s found %d", filter_ == 0 ? "none" : arena::tag_name(kFilters[filter_]),
                 found_count_);
}

void ShowcaseScene::update() {
    namespace input = nnh::input;
    using nnh::Button;
    if (input::pressed(Button::Start)) {
        nnh::scenes::pop();
        return;
    }
    if (input::pressed(Button::Select)) {
        nnh::scenes::replace(*demos_.check);
        return;
    }
    if (input::pressed(Button::Y)) {
        nnh::scenes::replace(*demos_.bench);
        return;
    }
    if (input::pressed(Button::X)) cycle_filter();
    if (input::pressed(Button::L) && filter_ != 0)
        NNH_LOG_INFO(example, "freeze: %s %d", arena::tag_name(kFilters[filter_]), arena_.freeze(kFilters[filter_]));
    if (input::pressed(Button::R)) NNH_LOG_INFO(example, "unfreeze: %d", arena_.unfreeze());
    if (input::pressed(Button::B)) arena_.zap_target();
    if (input::touch_started()) {
        const nnh::TouchPoint t = input::touch_position();
        arena_.destroy_at({kBottomCamera.x + static_cast<float>(t.x), kBottomCamera.y + static_cast<float>(t.y)});
    }
    arena_.update(stick(), input::held(Button::A));
    refresh_filter(); // the outlines follow destroys and spawns
}

void ShowcaseScene::draw() {
    namespace gfx = nnh::gfx;
    arena::draw_arena(nnh::Screen::Top, arena_, found_, found_count_);
    arena::draw_arena(nnh::Screen::Bottom, arena_, found_, found_count_);
    gfx::draw_text(nnh::Screen::Top, {200, 4}, "ENTITIES, TAGS AND POOLS",
                   {.color = nnh::rgb(250, 220, 120), .align = nnh::TextAlign::Center, .shadow = true});
    arena::draw_hud(arena_, kFilters[filter_], found_count_);
}
