#include "showcase_scene.hpp"

#include <cstring>
#include <new>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>
#include <nnh/tilemap.hpp>

#include "hud.hpp"
#include "minimap.hpp"

void ShowcaseScene::on_enter() {
    hero_.reset(new (std::nothrow) Hero());
    crowd_.reset(new (std::nothrow) Crowd());
    if (!hero_ || !crowd_ || !hero_->init(level_) || !crowd_->init(level_)) {
        hero_.reset();
        crowd_.reset();
    }
    mode_ = debug_view::Mode::Off;
    frame_ = 0;
    NNH_LOG_INFO(example, "pathfinding: showcase");
}

void ShowcaseScene::on_exit() {
    hero_.reset();
    crowd_.reset();
}

void ShowcaseScene::controls() {
    namespace input = nnh::input;
    using nnh::Button;
    if (input::touch_started()) {
        const nnh::TouchPoint t = input::touch_position();
        nnh::CellPos cell;
        if (minimap::touched_cell(level_, t.x, t.y, cell)) hero_->set_goal(level_, cell);
    }
    if (input::pressed(Button::A)) {
        const int d = next_door_;
        next_door_ = (next_door_ + 1) % Level::kDoors;
        level_.toggle(d);
        NNH_LOG_INFO(example, "door: %d %s version %u", d + 1, level_.open[d] ? "open" : "closed",
                     static_cast<unsigned>(level_.small.version()));
    }
    if (input::pressed(Button::X)) hero_->set_big(level_, !hero_->big());
    if (input::pressed(Button::L)) {
        const auto next = static_cast<nnh::Diagonals>((static_cast<int>(level_.small.diagonals()) + 1) % 4);
        level_.set_diagonals(next);
        NNH_LOG_INFO(example, "mode: %s", diagonals_name(next));
    }
    if (input::pressed(Button::R)) hero_->set_smoothing(level_, !hero_->smoothing());
    if (input::pressed(Button::Up)) mode_ = static_cast<debug_view::Mode>((static_cast<int>(mode_) + 1) % 3);
    if (input::pressed(Button::Down)) {
        hero_->set_paused(!hero_->paused());
        NNH_LOG_INFO(example, "hero: %s", hero_->paused() ? "paused" : "walking");
    }
}

void ShowcaseScene::shot_preset() {
    const char* shot = shot_name();
    if (shot[0] == '\0') return;
    mode_ = debug_view::Mode::Search;
    nnh::CellPos goal{44, 27};
    if (std::strcmp(shot, "explored") == 0) hero_->set_smoothing(level_, false);
    if (std::strcmp(shot, "field") == 0) mode_ = debug_view::Mode::Field;
    if (std::strcmp(shot, "partial") == 0) goal = {34, 7};
    if (std::strcmp(shot, "door-closed") == 0 || std::strcmp(shot, "door-open") == 0) goal = {24, 5};
    if (std::strcmp(shot, "door-closed") == 0) level_.toggle(0);
    if (std::strcmp(shot, "big") == 0) {
        hero_->set_big(level_, true);
        goal = {24, 20};
    }
    hero_->set_paused(true); // the picture shows the plan from the spawn
    hero_->set_goal(level_, goal);
}

void ShowcaseScene::follow_camera() {
    const nnh::Vec2 c = hero_->center();
    const float w = static_cast<float>(level_.width() * Level::kTile);
    const float h = static_cast<float>(level_.height() * Level::kTile);
    float x = c.x - 200;
    float y = c.y - 120;
    x = x < 0 ? 0 : (x > w - 400 ? w - 400 : x);
    y = y < 0 ? 0 : (y > h - 240 ? h - 240 : y);
    camera_ = {static_cast<float>(static_cast<int>(x)), static_cast<float>(static_cast<int>(y))};
    nnh::gfx::set_camera(nnh::Screen::Top, {.position = camera_});
    nnh::gfx::set_camera(nnh::Screen::Bottom, {});
}

void ShowcaseScene::update() {
    if (nnh::input::pressed(nnh::Button::Start)) {
        nnh::scenes::pop();
        return;
    }
    if (nnh::input::pressed(nnh::Button::Select)) {
        nnh::scenes::replace(*demos_.check);
        return;
    }
    if (nnh::input::pressed(nnh::Button::Y)) {
        nnh::scenes::replace(*demos_.bench);
        return;
    }
    if (!hero_) return;
    // The hero first: a change made by this frame's controls (a door, the mode) is re-planned in the next frame, so
    // its line follows the change's line (a script can expect each).
    hero_->update(level_);
    crowd_->update(level_, level_.small.cell_at(hero_->center()));
    if (++frame_ == 5) shot_preset();
    controls();
    follow_camera();
}

void ShowcaseScene::draw() {
    namespace gfx = nnh::gfx;
    if (!hero_) {
        gfx::draw_text(nnh::Screen::Top, {8, 8}, "levels/keep failed to load (see the log)");
        return;
    }
    gfx::draw_tilemap(nnh::Screen::Top, level_.map);
    minimap::draw_level(level_);
    const nnh::PathMap& map = hero_->map(level_);
    const int x0 = static_cast<int>(camera_.x) / Level::kTile;
    const int y0 = static_cast<int>(camera_.y) / Level::kTile;
    const int x1 = x0 + 25 < level_.width() - 1 ? x0 + 25 : level_.width() - 1;
    const int y1 = y0 + 15 < level_.height() - 1 ? y0 + 15 : level_.height() - 1;
    const debug_view::Surface top{nnh::Screen::Top, {}, Level::kTile, x0, y0, x1, y1};
    const debug_view::Surface small{nnh::Screen::Bottom, {minimap::kLeft, minimap::kTop}, minimap::kCell, 0, 0,
                                    level_.width() - 1, level_.height() - 1};
    const float scale = static_cast<float>(minimap::kCell) / Level::kTile;
    if (mode_ == debug_view::Mode::Field) {
        debug_view::draw_field(top, crowd_->field());
        debug_view::draw_field(small, crowd_->field());
    } else if (mode_ == debug_view::Mode::Search) {
        debug_view::draw_search(top, hero_->searcher(), hero_->cells(), hero_->cell_count(), hero_->points(),
                                hero_->point_count(), 1);
        debug_view::draw_search(small, hero_->searcher(), hero_->cells(), hero_->cell_count(), hero_->points(),
                                hero_->point_count(), scale);
    } else if (hero_->point_count() > 1) { // the waypoints, always on the minimap
        for (int i = 1; i < hero_->point_count(); ++i) {
            const nnh::Vec2 a = small.origin + hero_->points()[i - 1] * scale;
            const nnh::Vec2 b = small.origin + hero_->points()[i] * scale;
            gfx::draw_line(nnh::Screen::Bottom, a, b, nnh::rgb(255, 90, 200), {.layer = 3});
        }
    }
    for (int i = 0; i < Crowd::kChasers; ++i) {
        const nnh::Rect b = crowd_->boxes()[i];
        gfx::fill_rect(nnh::Screen::Top, b, nnh::rgb(220, 60, 60), {.layer = 4});
        gfx::fill_rect(nnh::Screen::Bottom, {small.origin.x + b.x * scale, small.origin.y + b.y * scale, 3, 3},
                       nnh::rgb(220, 60, 60), {.layer = 4});
    }
    const nnh::Rect h = hero_->box();
    gfx::fill_rect(nnh::Screen::Top, h, nnh::rgb(90, 220, 120), {.layer = 5});
    gfx::draw_rect(nnh::Screen::Top, h, nnh::rgb(20, 60, 30), {.layer = 5});
    gfx::fill_rect(nnh::Screen::Bottom, {small.origin.x + h.x * scale, small.origin.y + h.y * scale,
                                         h.w * scale + 1, h.h * scale + 1},
                   nnh::rgb(90, 220, 120), {.layer = 5});
    if (hero_->has_goal()) {
        const nnh::Vec2 g = map.cell_center(hero_->goal());
        gfx::draw_rect(nnh::Screen::Top, {g.x - 6, g.y - 6, 12, 12}, nnh::rgb(255, 255, 0), {.layer = 6});
        const nnh::Rect m = minimap::cell_rect(hero_->goal().x, hero_->goal().y);
        gfx::draw_rect(nnh::Screen::Bottom, m, nnh::rgb(255, 255, 0), {.layer = 6});
    }
    draw_hud(level_, *hero_, *crowd_, mode_);
}
