#include "bestiary_scene.hpp"

#include <cstdio>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>
#include <nnh/log.hpp>
#include <utility>

NNH_LOG_CHANNEL(bestiary);

namespace data = nnh::data;
using nnh::Screen;

namespace {

const char* element_name(Element e) {
    switch (e) {
    case Element::Fire:
        return "Fire";
    case Element::Ice:
        return "Ice";
    case Element::Earth:
        return "Earth";
    default:
        return "Normal";
    }
}

/// Media named by a data asset are loaded by the game, with the path the reference turns into.
nnh::Texture texture(data::TextureRef ref) {
    auto t = nnh::load_texture(data::path(ref));
    if (t) return std::move(t.value());
    NNH_LOG_WARN(bestiary, "%s", t.error().message);
    return {};
}

} // namespace

void BestiaryScene::load_all() {
    if (auto r = data::load("game", settings_); !r) NNH_LOG_ERROR(bestiary, "%s", r.error().message);
    enemy_count_ = data::count<Enemy>() < kMax ? data::count<Enemy>() : kMax;
    for (int i = 0; i < enemy_count_; ++i) {
        if (auto r = data::load(data::ref<Enemy>(i), enemies_[i]); !r) NNH_LOG_ERROR(bestiary, "%s", r.error().message);
        sprites_[i] = texture(enemies_[i].sprite);
        if (auto s = nnh::load_sound(data::path(enemies_[i].hurt_sound))) hurt_[i] = std::move(s.value());
        if (data::ref<Enemy>(i) == settings_.first) selected_ = i;
    }
    item_count_ = data::count<Item>() < kMax ? data::count<Item>() : kMax;
    for (int i = 0; i < item_count_; ++i) {
        item_refs_[i] = data::ref<Item>(i);
        if (auto r = data::load(item_refs_[i], items_[i]); !r) NNH_LOG_ERROR(bestiary, "%s", r.error().message);
        icons_[i] = texture(items_[i].icon);
    }
    loaded_ = true;
}

void BestiaryScene::on_enter() {
    if (!loaded_) {
        load_all();
        if (auto m = nnh::music::play(data::path(settings_.music), {.fade_in = 0.5f}); !m) {
            NNH_LOG_WARN(bestiary, "%s", m.error().message);
        }
        NNH_LOG_INFO(bestiary, "bestiary: %d enemies, %d items, data index %u bytes", enemy_count_, item_count_,
                     static_cast<unsigned>(data::memory_used()));
    }
    nnh::gfx::set_clear_color(Screen::Top, settings_.top_color);
    nnh::gfx::set_clear_color(Screen::Bottom, settings_.bottom_color);
}

void BestiaryScene::update() {
    if (enemy_count_ > 0 && nnh::input::pressed(nnh::Button::Down)) selected_ = (selected_ + 1) % enemy_count_;
    if (enemy_count_ > 0 && nnh::input::pressed(nnh::Button::Up))
        selected_ = (selected_ + enemy_count_ - 1) % enemy_count_;
    if (nnh::input::pressed(nnh::Button::A) && hurt_[selected_].valid()) nnh::audio::play(hurt_[selected_]);
    if (nnh::input::pressed(nnh::Button::Select)) nnh::scenes::push(check_);
    if (nnh::input::pressed(nnh::Button::Start)) nnh::scenes::pop();
}

int BestiaryScene::item_index(data::Ref<Item> ref) const {
    for (int i = 0; i < item_count_; ++i) {
        if (item_refs_[i] == ref) return i;
    }
    return -1;
}

void BestiaryScene::draw_enemy() const {
    const Enemy& e = enemies_[selected_];
    nnh::gfx::draw_text(Screen::Top, {200, 8}, settings_.title.c_str(),
                        {.scale = 2, .align = nnh::TextAlign::Center, .shadow = true});
    nnh::gfx::fill_rect(Screen::Top, {24, 52, 136, 136}, nnh::rgba(0, 0, 0, 90));
    if (sprites_[selected_].valid()) nnh::gfx::draw(Screen::Top, sprites_[selected_], {44, 72}, {.scale = {3, 3}, .tint = e.tint});
    nnh::gfx::draw_text(Screen::Top, {180, 52}, e.name.c_str(), {.color = e.tint, .scale = 2});
    char line[96];
    std::snprintf(line, sizeof line, "HP %u  ATK %u  DEF %u", e.stats.hp, e.stats.attack, e.stats.defense);
    nnh::gfx::draw_text(Screen::Top, {180, 80}, line);
    std::snprintf(line, sizeof line, "Speed %.2f\nElement %s\nSpawn (%.0f, %.0f)", static_cast<double>(e.speed),
                  element_name(e.element), static_cast<double>(e.spawn.x), static_cast<double>(e.spawn.y));
    nnh::gfx::draw_text(Screen::Top, {180, 96}, line);
    std::snprintf(line, sizeof line, "%s%s%s", e.flags[0] ? "flying " : "", e.flags[1] ? "boss " : "",
                  e.flags[2] ? "undead" : "");
    nnh::gfx::draw_text(Screen::Top, {180, 144}, line, {.color = nnh::colors::yellow});
    std::snprintf(line, sizeof line, "%s  %s", data::path(e.sprite), data::path(e.hurt_sound));
    nnh::gfx::draw_text(Screen::Top, {200, 220}, line, {.color = nnh::colors::gray, .align = nnh::TextAlign::Center});
}

void BestiaryScene::draw_list() const {
    char line[96];
    std::snprintf(line, sizeof line, "BESTIARY  %d enemies", enemy_count_);
    nnh::gfx::draw_text(Screen::Bottom, {8, 6}, line, {.color = nnh::colors::yellow});
    for (int i = 0; i < enemy_count_; ++i) {
        const float y = 22.0f + static_cast<float>(i) * 12;
        if (i == selected_) nnh::gfx::fill_rect(Screen::Bottom, {4, y - 2, 150, 12}, nnh::rgba(255, 255, 255, 50));
        std::snprintf(line, sizeof line, "%s %s", i == selected_ ? ">" : " ", data::name<Enemy>(i));
        nnh::gfx::draw_text(Screen::Bottom, {8, y}, line);
    }
    const Enemy& e = enemies_[selected_];
    nnh::gfx::draw_text(Screen::Bottom, {8, 92}, "DROPS", {.color = nnh::colors::yellow});
    float y = 108;
    for (const Drop& d : e.drops) {
        const int k = item_index(d.item);
        if (k < 0) continue;
        if (icons_[k].valid()) nnh::gfx::draw(Screen::Bottom, icons_[k], {8, y - 4});
        std::snprintf(line, sizeof line, "%-12s %3.0f%%  %5d G", items_[k].name.c_str(),
                      static_cast<double>(d.chance * 100), static_cast<int>(items_[k].price));
        nnh::gfx::draw_text(Screen::Bottom, {30, y}, line);
        if (!items_[k].summons.empty()) {
            std::snprintf(line, sizeof line, "summons %s", data::path(items_[k].summons));
            nnh::gfx::draw_text(Screen::Bottom, {30, y + 10}, line, {.color = nnh::colors::gray});
            y += 10;
        }
        y += 22;
    }
    nnh::gfx::draw_text(Screen::Bottom, {160, 226}, "UP/DOWN  A sound  SELECT check",
                        {.color = nnh::colors::gray, .align = nnh::TextAlign::Center});
}

void BestiaryScene::draw() {
    if (enemy_count_ == 0) {
        nnh::gfx::draw_text(Screen::Top, {200, 110}, "no enemies: see the log", {.align = nnh::TextAlign::Center});
        return;
    }
    draw_enemy();
    draw_list();
}
