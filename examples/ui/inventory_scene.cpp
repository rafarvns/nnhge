#include "inventory_scene.hpp"

#include "items.hpp"
#include "names.hpp"

namespace {

void fill_row(int index, nnh::UiRow& row) {
    row.text = item_name(index);
    row.number = item_count(index);
    row.number_prefix = "x";
    row.icon_texture = 0;
    row.icon = item_icon(index);
}

} // namespace

bool InventoryScene::create() {
    if (!make_ui({.widgets = 24, .text_bytes = 32, .rows = 16, .functions = 4})) return false;
    const nnh::UiWidget top = ui_.root(nnh::Screen::Top);
    const nnh::UiWidget card = ui_.panel(top, {.as_box = true, .spacing = 6,
                                               .layout = {.width = 300, .place = {.anchor = nnh::UiAnchor::Center, .y = 10}}});
    name_ = ui_.label(card, {.text = nnh::UiText::borrow(item_name(0))});
    text_ = ui_.label(card, {.text = nnh::UiText::borrow(item_description(0)), .wrap = true});
    list_ = ui_.list(ui_.root(nnh::Screen::Bottom),
                     {.rows = 13, .count = kItems, .fill = &fill_row, .margin = 2, .cell_width = 280, .key = Key::List,
                      .layout = {.place = {.anchor = nnh::UiAnchor::Center}}});
    return true;
}

void InventoryScene::on_enter() {
    ui_.focus(list_);
    if (shot_is("list")) ui_.set_item(list_, 250);
    describe(ui_.value(list_));
}

void InventoryScene::describe(int item) {
    ui_.set_text(name_, nnh::UiText::borrow(item_name(item)));
    ui_.set_text(text_, nnh::UiText::borrow(item_description(item)));
}

void InventoryScene::step() {
    for (const nnh::UiResult& r : ui_.results()) {
        if (r.kind == nnh::UiResultKind::FocusChanged && r.widget == list_) describe(r.index);
    }
    if (ui_.cancelled()) nnh::scenes::pop();
}
