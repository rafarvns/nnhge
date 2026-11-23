#include "queries_scene.hpp"

#include "hud.hpp"
#include "query_draw.hpp"

#include <cstdio>
#include <cstring>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>

namespace {

using nnh::Screen;
namespace collision = nnh::collision;
namespace gfx = nnh::gfx;
using room::bits::kBlue;
using room::bits::kGreen;
using room::bits::kRed;
using room::bits::kWater;

enum DrawLayer : int { kTiles = 0, kBoxes = 2, kCells = 3, kRays = 4, kHits = 5 };

nnh::Vec2 direction() {
    const nnh::Stick pad = nnh::input::circle_pad();
    nnh::Vec2 d{pad.x, -pad.y};
    if (nnh::input::held(nnh::Button::Left)) d.x = -1;
    if (nnh::input::held(nnh::Button::Right)) d.x = 1;
    if (nnh::input::held(nnh::Button::Up)) d.y = -1;
    if (nnh::input::held(nnh::Button::Down)) d.y = 1;
    return d;
}

const char* kind_name(collision::Cell kind) {
    return kind == collision::Cell::Solid ? "solid" : (kind == collision::Cell::OneWay ? "one-way" : "empty");
}

const char* mode_name(QueriesScene::Mode mode) {
    return mode == QueriesScene::Mode::First ? "first" : (mode == QueriesScene::Mode::All ? "all" : "cast");
}

} // namespace

QueriesScene::QueriesScene(const Demos& demos, room::Room& room) : demos_(demos), room_(room) {}

void QueriesScene::preset() {
    if (demo_is("all")) {
        mode_ = Mode::All;
        target_ = {372, 168};
    }
    if (demo_is("cast")) {
        mode_ = Mode::Cast;
        target_ = {352, 100};
    }
    if (demo_is("water")) {
        mask_ &= ~kWater;
        target_ = {150, 216};
    }
    if (demo_is("touch")) ask_under({kBottomCamera.x + 72, 152}); // the pool's top-left water cell
}

void QueriesScene::on_enter() {
    gfx::set_camera(Screen::Top, {});
    gfx::set_camera(Screen::Bottom, {kBottomCamera});
    first_line_[0] = '\0';
    run_queries();
    NNH_LOG_INFO(example, "queries: playground, mode %s, mask 0x%X", mode_name(mode_), static_cast<unsigned>(mask_));
}

collision::QueryFilter QueriesScene::filter() const {
    return {.mask = mask_, .ignore_one_way = ignore_one_way_};
}

void QueriesScene::handle_buttons() {
    using nnh::Button;
    namespace input = nnh::input;
    if (input::pressed(Button::X)) {
        mode_ = mode_ == Mode::First ? Mode::All : (mode_ == Mode::All ? Mode::Cast : Mode::First);
        NNH_LOG_INFO(example, "queries: mode %s", mode_name(mode_));
    }
    const collision::Layers before = mask_;
    if (input::pressed(Button::A)) mask_ ^= kWater;
    if (input::pressed(Button::B)) mask_ ^= kRed;
    if (input::pressed(Button::L)) mask_ ^= kGreen;
    if (input::pressed(Button::R)) mask_ ^= kBlue;
    if (mask_ != before) NNH_LOG_INFO(example, "queries: mask 0x%X", static_cast<unsigned>(mask_));
    if (input::pressed(Button::Y)) {
        ignore_one_way_ = !ignore_one_way_;
        NNH_LOG_INFO(example, "queries: one-way %s", ignore_one_way_ ? "ignored" : "blocks");
    }
}

void QueriesScene::move_target() {
    target_ += direction() * 2.0f;
    const float w = static_cast<float>(room::kWidth * room::kTile) - 1;
    const float h = static_cast<float>(room::kHeight * room::kTile) - 1;
    target_.x = target_.x < 0 ? 0 : (target_.x > w ? w : target_.x);
    target_.y = target_.y < 0 ? 0 : (target_.y > h ? h : target_.y);
}

void QueriesScene::update() {
    handle_buttons();
    move_target();
    if (nnh::input::touching()) {
        const nnh::TouchPoint t = nnh::input::touch_position();
        const nnh::Vec2 world{kBottomCamera.x + static_cast<float>(t.x), kBottomCamera.y + static_cast<float>(t.y)};
        if (nnh::input::touch_started() || world.x != touch_.x || world.y != touch_.y) ask_under(world);
    }
    run_queries();
    if (nnh::input::pressed(nnh::Button::Select)) nnh::scenes::replace(*demos_.check);
    if (nnh::input::pressed(nnh::Button::Start)) nnh::scenes::pop();
}

void QueriesScene::run_queries() {
    const collision::Grid grid = room_.grid(mask_);
    const collision::BoxView boxes = room_.view();
    has_first_ = collision::raycast(grid, boxes, room::kEmitter, target_, first_, filter());
    hit_count_ = cell_count_ = 0;
    has_cast_ = false;
    if (mode_ == Mode::All) {
        cell_count_ = collision::cells_crossed(grid, room::kEmitter, target_, cells_, kMaxCells);
        const int n = collision::raycast_all(grid, room::kEmitter, target_, hits_, kMaxHits / 2, filter());
        const int kept = n < kMaxHits / 2 ? n : kMaxHits / 2;
        const int m = collision::raycast_all(boxes, room::kEmitter, target_, hits_ + kept, kMaxHits - kept, filter());
        hit_count_ = kept + (m < kMaxHits - kept ? m : kMaxHits - kept);
        if (cell_count_ > kMaxCells) cell_count_ = kMaxCells;
    }
    if (mode_ == Mode::Cast) {
        const nnh::Rect box{room::kEmitter.x - kCastSize.x / 2, room::kEmitter.y - kCastSize.y / 2, kCastSize.x,
                            kCastSize.y};
        has_cast_ = collision::box_cast(boxes, box, target_ - kCastSize * 0.5f, cast_, filter());
    }
    log_first_hit();
}

void QueriesScene::log_first_hit() {
    char line[96];
    if (!has_first_) {
        std::snprintf(line, sizeof line, "queries: first hit none");
    } else if (first_.box >= 0) {
        std::snprintf(line, sizeof line, "queries: first hit box %d %s fraction %.4f normal %d,%d", first_.box,
                      room::thing_name(room_.things[first_.box].layers), static_cast<double>(first_.fraction),
                      static_cast<int>(first_.normal.x), static_cast<int>(first_.normal.y));
    } else {
        std::snprintf(line, sizeof line, "queries: first hit cell %d,%d %s fraction %.4f normal %d,%d", first_.cell.x,
                      first_.cell.y, kind_name(first_.cell.kind), static_cast<double>(first_.fraction),
                      static_cast<int>(first_.normal.x), static_cast<int>(first_.normal.y));
    }
    if (std::strcmp(line, first_line_) == 0) return;
    std::strcpy(first_line_, line);
    NNH_LOG_INFO(example, "%s", first_line_);
}

void QueriesScene::ask_under(nnh::Vec2 world) {
    has_touch_ = true;
    touch_ = world;
    const nnh::Tilemap& map = room_.map;
    const collision::CellHit cell = collision::cell_at(map.collision_grid(), world);
    int tile = 0;
    const char* name = "";
    for (int layer = map.tile_layer_count() - 1; layer >= 0 && tile == 0; --layer) {
        tile = map.tile_at(layer, world).id; // the topmost tile under the point
    }
    if (tile != 0) name = map.properties(nnh::PropertyOwner::tile(tile)).get_string("name");
    int boxes[room::kThings];
    const int n = collision::boxes_at(room_.view(), world, boxes, room::kThings, filter());
    std::snprintf(under_line_, sizeof under_line_, "under: cell %d,%d %s layers 0x%X tile %d boxes %d", cell.x, cell.y,
                  kind_name(cell.kind), static_cast<unsigned>(map.collision_layers_at(cell.x, cell.y)), tile, n);
    std::snprintf(under_cell_, sizeof under_cell_, "cell %d,%d %s, layers 0x%X", cell.x, cell.y, kind_name(cell.kind),
                  static_cast<unsigned>(map.collision_layers_at(cell.x, cell.y)));
    int at = std::snprintf(under_detail_, sizeof under_detail_, "tile %d \"%s\"", tile, name);
    for (int i = 0; i < n && at > 0 && at < static_cast<int>(sizeof under_detail_); ++i) {
        at += std::snprintf(under_detail_ + at, sizeof under_detail_ - static_cast<std::size_t>(at), "%s box %d %s",
                            i == 0 ? ";" : ",", boxes[i], room::thing_name(room_.things[boxes[i]].layers));
    }
    NNH_LOG_INFO(example, "%s", under_line_);
}

void QueriesScene::draw_room(Screen screen) const {
    gfx::draw_tilemap(screen, room_.map, {.layer = kTiles});
    for (const room::Thing& t : room_.things) {
        const bool seen = collision::detects(mask_, t.layers);
        gfx::fill_rect(screen, t.box, seen ? room::thing_color(t.layers) : nnh::rgba(120, 120, 120, 120),
                       {.layer = kBoxes});
    }
}

void QueriesScene::draw_results(Screen screen) const {
    const collision::Grid grid = room_.grid(mask_);
    query_draw::cells(screen, grid, cells_, cell_count_, kCells);
    const nnh::Vec2 end = has_first_ && mode_ == Mode::First ? first_.point : target_;
    query_draw::ray(screen, room::kEmitter, end, nnh::rgb(255, 220, 120), kRays);
    if (mode_ == Mode::First) gfx::draw_line(screen, end, target_, nnh::rgba(255, 220, 120, 90), {.layer = kRays});
    for (int i = 0; i < hit_count_; ++i) query_draw::hit(screen, hits_[i], false, kHits);
    if (has_first_ && mode_ != Mode::Cast) query_draw::hit(screen, first_, true, kHits);
    if (mode_ == Mode::Cast) {
        const nnh::Vec2 to = target_ - kCastSize * 0.5f;
        query_draw::cast_box(screen, to, kCastSize, nnh::rgba(255, 255, 255, 120), kRays);
        if (has_cast_) {
            query_draw::cast_box(screen, cast_.point, kCastSize, nnh::colors::yellow, kHits);
            gfx::draw_line(screen, cast_.point + kCastSize * 0.5f, cast_.point + kCastSize * 0.5f + cast_.normal * 12.0f,
                           nnh::rgb(0, 230, 230), {.layer = kHits});
        }
    }
    gfx::fill_rect(screen, {target_.x - 3, target_.y - 3, 7, 7}, nnh::rgb(255, 80, 200), {.layer = kHits});
    if (has_touch_) gfx::draw_rect(screen, {touch_.x - 4, touch_.y - 4, 9, 9}, nnh::rgb(255, 60, 200), {.layer = kHits});
}

void QueriesScene::draw() {
    draw_room(Screen::Top);
    draw_results(Screen::Top);
    draw_room(Screen::Bottom);
    if (has_touch_) {
        gfx::fill_rect(Screen::Bottom, {touch_.x - 1, touch_.y - 6, 3, 13}, nnh::rgb(255, 60, 200), {.layer = kHits});
        gfx::fill_rect(Screen::Bottom, {touch_.x - 6, touch_.y - 1, 13, 3}, nnh::rgb(255, 60, 200), {.layer = kHits});
    }
    draw_hud(*this);
}
