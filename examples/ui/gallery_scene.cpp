#include "gallery_scene.hpp"

#include <nnh/gfx.hpp>

#include "items.hpp"
#include "names.hpp"

namespace {

void fill_cell(int index, nnh::UiRow& row) {
    row.icon_texture = 0;
    row.icon = item_icon(index * 3);
}

} // namespace

bool GalleryScene::create() {
    if (!make_ui({.widgets = 8, .rows = 20, .functions = 2})) return false;
    grid_ = ui_.list(ui_.root(nnh::Screen::Bottom),
                     {.rows = 4, .columns = 5, .count = 48, .fill = &fill_cell, .margin = 0, .row_height = 48,
                      .cell_width = 56, .key = Key::Grid, .layout = {.place = {.anchor = nnh::UiAnchor::Center}}});
    return true;
}

void GalleryScene::on_enter() { ui_.focus(grid_); }

void GalleryScene::step() {
    if (ui_.cancelled()) nnh::scenes::pop();
}

void GalleryScene::paint() const {
    if (demo_.art == nullptr) return;
    const int item = ui_.list_info(grid_).focused;
    if (item < 0) return;
    nnh::gfx::draw(nnh::Screen::Top, *demo_.art, {168, 88},
                   {.region = item_icon(item * 3), .scale = {4, 4}, .layer = 12, .ignore_camera = true});
}
