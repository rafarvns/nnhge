#include "options_scene.hpp"

#include "names.hpp"
#include "texts.hpp"

using nnh::UiText;

namespace {

UiText t(Text x) { return UiText::key(x); }

const UiText kLevels[3] = {UiText::key(Text::Easy), UiText::key(Text::Normal), UiText::key(Text::Hard)};
const UiText kSpeeds[2] = {UiText::key(Text::Slow), UiText::key(Text::Fast)};
const UiText kLanguages[2] = {UiText::key(Text::English), UiText::key(Text::Portuguese)};

} // namespace

bool OptionsScene::create() {
    if (!make_ui({.widgets = 48, .text_bytes = 64, .texts = 8, .functions = 4, .top = true})) return false;
    const nnh::UiWidget top = ui_.root(nnh::Screen::Top);
    ui_.label(top, {.text = t(Text::OptionsTitle), .layout = {.place = {.anchor = nnh::UiAnchor::Center}}});
    panel_ = ui_.panel(ui_.root(nnh::Screen::Bottom),
                       {.as_box = true, .spacing = 4,
                        .layout = {.place = {.x = 8, .y = 8, .fill_x = true, .fill_y = true, .right = 8, .bottom = 8}}});
    box_ = ui_.scroll_box(panel_, {.spacing = 3, .margin = 1, .layout = {.expand = true}});
    const nnh::UiWidget box = box_;
    music_ = ui_.slider(box, {.text = t(Text::Music), .min = 0, .max = 10, .value = 7, .key = Key::Music});
    ui_.slider(box, {.text = t(Text::Sound), .min = 0, .max = 10, .value = 5, .key = Key::Sound});
    ui_.toggle(box, {.text = t(Text::InvertY), .key = Key::Invert});
    ui_.choice(box, {.text = t(Text::Difficulty), .options = kLevels, .count = 3, .index = 1, .key = Key::Difficulty});
    ui_.choice(box, {.text = t(Text::TextSpeed), .options = kSpeeds, .count = 2, .index = 1, .wrap = false,
                     .key = Key::Speed});
    ui_.choice(box, {.text = t(Text::Language), .options = kLanguages, .count = 2, .key = Key::Language});
    const nnh::UiWidget locked = ui_.hbox(box, {.spacing = 0, .disabled_focusable = true});
    const nnh::UiWidget online = ui_.button(locked, {.text = t(Text::Online), .key = Key::Online,
                                                     .layout = {.expand = true}});
    ui_.set_enabled(online, false);
    const Text more[6] = {Text::Vibration, Text::Subtitles, Text::Hints, Text::Autosave, Text::Clock, Text::Colours};
    const Key keys[6] = {Key::Vibration, Key::Subtitles, Key::Hints, Key::Autosave, Key::Clock, Key::Colours};
    for (int i = 0; i < 6; ++i) ui_.toggle(box, {.text = t(more[i]), .checked = i % 2 == 0, .key = keys[i]});
    back_ = ui_.button(box, {.text = t(Text::Back), .key = Key::OptionsBack});
    ui_.set_back(box, back_); // B activates Back
    return true;
}

void OptionsScene::on_enter() {
    ui_.focus(music_);
    ui_.set_scroll(box_, 0);
    // slide in from the right: a tween writing the panel's render offset through a setter (no relayout)
    tweens_.from_to(nnh::Setter<float>{[this](float x) { ui_.set_render_offset(panel_, {x, 0}); }}, 320.0f, 0.0f,
                    nnh::Frames{20}, {.ease = nnh::Ease::CubicOut});
}

void OptionsScene::step() {
    if (ui_.changed(Key::Language)) {
        demo_.language = ui_.value(Key::Language);
        NNH_LOG_INFO(example, "ui: language %s", demo_.language == 0 ? "en" : "pt");
    }
    if (ui_.activated(Key::OptionsBack)) nnh::scenes::pop();
}
