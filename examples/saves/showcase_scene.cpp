// The saves example's showcase (showcase_scene.hpp).

#include "showcase_scene.hpp"

#include "hud.hpp"
#include "progress_sum.hpp"

#include <cstdio>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>

using namespace saves_example;

ShowcaseScene::ShowcaseScene(const Demos& demos) : demos_(demos) {
    game_.reset();
}

void ShowcaseScene::on_enter() {
    nnh::gfx::set_camera(nnh::Screen::Top, {});
    nnh::gfx::set_camera(nnh::Screen::Bottom, {});
    frame_ = 0;
    settings_ = Settings{}; // a failed load leaves the struct as it was: start from the defaults, not the last entry's
    (void)load_settings(settings_);
    refresh_slots();
    char a[8], b[8], c[8];
    format_slot_short(a, sizeof a, slots_[0]);
    format_slot_short(b, sizeof b, slots_[1]);
    format_slot_short(c, sizeof c, slots_[2]);
    NNH_LOG_INFO(example, "saves: showcase slots %s %s %s", a, b, c);
}

void ShowcaseScene::refresh_slots() {
    for (int i = 0; i < kSlots; ++i) {
        SlotView& s = slots_[i];
        s = SlotView{};
        const auto r = nnh::save::load(slot_path(i), s.summary, buffer_.bytes, sizeof buffer_.bytes);
        s.used = r.ok();
        s.from_backup = r.ok() && r.value().used_backup;
    }
}

void ShowcaseScene::store(int slot) {
    const auto size = nnh::save::encode(game_.progress(), buffer_.bytes, sizeof buffer_.bytes);
    const auto r = nnh::save::store(slot_path(slot), game_.progress(), buffer_);
    if (!size || !r) {
        NNH_LOG_WARN(example, "slot %d: store failed: %s", slot + 1, (r ? size.error() : r.error()).message);
        return;
    }
    NNH_LOG_INFO(example, "slot %d: stored %u bytes", slot + 1, static_cast<unsigned>(size.value()));
    refresh_slots();
}

void ShowcaseScene::load(int slot) {
    const auto r = nnh::save::load(slot_path(slot), game_.progress(), buffer_);
    if (!r) {
        std::snprintf(report_, sizeof report_, "slot %d: %s", slot + 1, r.error().message);
        NNH_LOG_WARN(example, "slot %d: load failed: %s", slot + 1, r.error().message);
        return;
    }
    const nnh::save::Report& rep = r.value();
    format_report(report_, sizeof report_, rep);
    NNH_LOG_INFO(example,
                 "slot %d: loaded sum %08lX version %lu steps %d unknown %d dropped %d adjusted %d%s",
                 slot + 1, static_cast<unsigned long>(progress_sum(game_.progress())),
                 static_cast<unsigned long>(rep.version), rep.migrated, rep.unknown, rep.dropped,
                 rep.adjusted, rep.used_backup ? " backup" : "");
}

void ShowcaseScene::remove(int slot) {
    if (nnh::save::remove(slot_path(slot))) NNH_LOG_INFO(example, "slot %d: deleted", slot + 1);
    refresh_slots();
}

void ShowcaseScene::damage(int slot) {
    if (damage_save(slot_path(slot), buffer_.bytes, sizeof buffer_.bytes)) {
        NNH_LOG_INFO(example, "slot %d: damaged", slot + 1);
    }
    refresh_slots();
}

void ShowcaseScene::load_golden() {
    const char* path = install_golden(1, buffer_.bytes, sizeof buffer_.bytes);
    const auto r = path != nullptr
                       ? nnh::save::load(path, game_.progress(), buffer_)
                       : nnh::Result<nnh::save::Report>(nnh::Error::make(nnh::ErrorCode::NotFound, "asset"));
    if (!r) {
        NNH_LOG_WARN(example, "golden v1: %s", r.error().message);
        return;
    }
    format_report(report_, sizeof report_, r.value());
    NNH_LOG_INFO(example, "golden v1: loaded sum %08lX steps %d",
                 static_cast<unsigned long>(progress_sum(game_.progress())), r.value().migrated);
}

void ShowcaseScene::change_settings(int volume_delta, bool next_language) {
    const int volume = settings_.volume + volume_delta;
    settings_.volume = static_cast<std::uint8_t>(volume < 0 ? 0 : volume > 10 ? 10 : volume);
    if (next_language) settings_.language = static_cast<std::uint8_t>((settings_.language + 1) % kLanguages);
    settings_.controls.clear();
    const std::uint8_t blob[] = {'N', 'N', 'H', 'B', 1, 0, settings_.volume, settings_.language};
    for (std::uint8_t b : blob)
        settings_.controls.push_back(b);
    if (store_settings(settings_)) {
        NNH_LOG_INFO(example, "settings: volume %d language %s stored", settings_.volume,
                     language_name(settings_.language));
    }
}

void ShowcaseScene::select(int slot) {
    selected_ = (slot + kSlots) % kSlots;
    NNH_LOG_INFO(example, "slot %d: selected", selected_ + 1);
}

void ShowcaseScene::act(PanelHit hit) {
    switch (hit.action) {
    case PanelAction::Save:
        store(hit.slot);
        break;
    case PanelAction::Load:
        load(hit.slot);
        break;
    case PanelAction::Delete:
        remove(hit.slot);
        break;
    case PanelAction::Damage:
        damage(hit.slot);
        break;
    case PanelAction::VolumeDown:
        change_settings(-1, false);
        break;
    case PanelAction::VolumeUp:
        change_settings(1, false);
        break;
    case PanelAction::Language:
        change_settings(0, true);
        break;
    case PanelAction::GoldenV1:
        load_golden();
        break;
    case PanelAction::Rename:
        NNH_LOG_INFO(example, "name: %s", game_.rename());
        break;
    case PanelAction::None:
        break;
    }
}

/// SAVES_SHOT: fixed actions at fixed frames, for the simulator's screenshots (taken at
/// NNH_EXIT_AFTER_FRAMES=30).
void ShowcaseScene::play_shot() {
    if (frame_ == 1) {
        for (int i = 0; i < kSlots; ++i)
            (void)nnh::save::remove(slot_path(i));
        refresh_slots();
    }
    if (shot_is("saved")) {
        if (frame_ == 2) store(0);
        if (frame_ == 3) {
            game_.rename();
            game_.progress().gems = 12;
            game_.progress().play_frames = 4000;
            store(1);
        }
        if (frame_ == 4) {
            game_.rename();
            game_.progress().level = 9;
            store(2);
        }
    } else if (shot_is("golden") && frame_ == 2) {
        load_golden();
    } else if (shot_is("backup")) {
        if (frame_ == 2) store(0);
        if (frame_ == 3) {
            game_.progress().gems += 10;
            store(0);
        }
        if (frame_ == 4) damage(0);
        if (frame_ == 5) load(0);
    }
}

void ShowcaseScene::update() {
    ++frame_;
    if (shot_is("empty") || shot_is("saved") || shot_is("golden") || shot_is("backup")) play_shot();
    game_.update();
    if (nnh::input::pressed(nnh::Button::L)) select(selected_ - 1);
    if (nnh::input::pressed(nnh::Button::R)) select(selected_ + 1);
    if (nnh::input::pressed(nnh::Button::X)) store(selected_);
    if (nnh::input::pressed(nnh::Button::A)) load(selected_);
    if (nnh::input::pressed(nnh::Button::B)) remove(selected_);
    if (nnh::input::touch_started()) {
        const nnh::TouchPoint t = nnh::input::touch_position();
        act(panel_hit(t.x, t.y));
    }
    if (nnh::input::pressed(nnh::Button::Y)) nnh::scenes::replace(*demos_.bench);
    if (nnh::input::pressed(nnh::Button::Select)) nnh::scenes::replace(*demos_.check);
    if (nnh::input::pressed(nnh::Button::Start)) nnh::scenes::pop();
}

void ShowcaseScene::draw() {
    game_.draw();
    draw_hud(game_.progress(), settings_.volume, language_name(settings_.language), report_);
    draw_panel(slots_, selected_);
}
