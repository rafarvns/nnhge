// Interactive widgets (spec 028 FR-6): button, toggle, slider, choice.

#include "ui/ui_block.hpp"
#include "ui/ui_widget_new.hpp"

namespace nnh {

UiWidget Ui::button(UiWidget parent, const UiButtonDesc& d) {
    detail::UiBlock* b = block_;
    if (b == nullptr) return {};
    if (d.icon_texture < -1 || d.icon_texture >= kMaxUiTextures) {
        NNH_UI_MISUSE(*b, "Ui::button: an icon texture slot outside -1..7");
        return {};
    }
    const int s = detail::widget_new(*b, parent, UiKind::Button, d.key, d.style, d.layout);
    if (s < 0) return {};
    detail::UiCold& c = b->cold[s];
    if (!detail::store_text(*b, d.text, c.text_kind, c.text)) {
        NNH_UI_DROP(*b, s, "Ui::button: the text bytes are full (UiDesc::text_bytes)");
        return {};
    }
    detail::UiButtonData& x = c.data.button;
    x = detail::UiButtonData{};
    x.icon_texture = static_cast<std::int8_t>(d.icon_texture);
    x.ix = static_cast<std::int16_t>(d.icon.x);
    x.iy = static_cast<std::int16_t>(d.icon.y);
    x.iw = static_cast<std::int16_t>(d.icon.w);
    x.ih = static_cast<std::int16_t>(d.icon.h);
    return detail::handle_of(*b, s);
}

UiWidget Ui::toggle(UiWidget parent, const UiToggleDesc& d) {
    detail::UiBlock* b = block_;
    if (b == nullptr) return {};
    const int s = detail::widget_new(*b, parent, UiKind::Toggle, d.key, d.style, d.layout);
    if (s < 0) return {};
    detail::UiCold& c = b->cold[s];
    if (!detail::store_text(*b, d.text, c.text_kind, c.text)) {
        NNH_UI_DROP(*b, s, "Ui::toggle: the text bytes are full (UiDesc::text_bytes)");
        return {};
    }
    c.data.toggle = detail::UiToggleData{};
    c.data.toggle.checked = d.checked ? 1 : 0;
    c.data.toggle.left_right = d.left_right_set ? 1 : 0;
    return detail::handle_of(*b, s);
}

UiWidget Ui::slider(UiWidget parent, const UiSliderDesc& d) {
    detail::UiBlock* b = block_;
    if (b == nullptr) return {};
    if (d.min > d.max || d.step < 1 || d.step > 32767) {
        NNH_UI_MISUSE(*b, "Ui::slider: min > max, or a step outside 1..32767");
        return {};
    }
    const int s = detail::widget_new(*b, parent, UiKind::Slider, d.key, d.style, d.layout);
    if (s < 0) return {};
    detail::UiCold& c = b->cold[s];
    if (!detail::store_text(*b, d.text, c.text_kind, c.text)) {
        NNH_UI_DROP(*b, s, "Ui::slider: the text bytes are full (UiDesc::text_bytes)");
        return {};
    }
    detail::UiSliderData& x = c.data.slider;
    x = detail::UiSliderData{};
    x.min = d.min;
    x.max = d.max;
    x.step = static_cast<std::int16_t>(d.step);
    x.value = d.value < d.min ? d.min : (d.value > d.max ? d.max : d.value);
    x.show_value = d.show_value ? 1 : 0;
    const int fn = detail::store_fn(*b, d.format);
    if (fn < 0) {
        NNH_UI_DROP(*b, s, "Ui::slider: the function slots are full (UiDesc::functions)");
        return {};
    }
    x.format = static_cast<std::uint8_t>(fn);
    return detail::handle_of(*b, s);
}

UiWidget Ui::choice(UiWidget parent, const UiChoiceDesc& d) {
    detail::UiBlock* b = block_;
    if (b == nullptr) return {};
    if (d.options == nullptr || d.count < 1 || d.count > 4096) {
        NNH_UI_MISUSE(*b, "Ui::choice: options must name 1..4096 texts");
        return {};
    }
    const int s = detail::widget_new(*b, parent, UiKind::Choice, d.key, d.style, d.layout);
    if (s < 0) return {};
    detail::UiCold& c = b->cold[s];
    if (!detail::store_text(*b, d.text, c.text_kind, c.text)) {
        NNH_UI_DROP(*b, s, "Ui::choice: the text bytes are full (UiDesc::text_bytes)");
        return {};
    }
    detail::UiChoiceData& x = c.data.choice;
    x = detail::UiChoiceData{};
    const int first = detail::texts_take(*b, d.count);
    if (first < 0) {
        NNH_UI_DROP(*b, s, "Ui::choice: the extra texts are full (UiDesc::texts)");
        return {};
    }
    x.first_text = static_cast<std::uint16_t>(first);
    for (int i = 0; i < d.count; ++i) {
        b->text_kinds[first + i] = detail::kUiTextNone;
        b->texts[first + i] = {};
    }
    x.count = static_cast<std::uint16_t>(d.count); // released with the widget from here on
    for (int i = 0; i < d.count; ++i) {
        if (!detail::store_text(*b, d.options[i], b->text_kinds[first + i], b->texts[first + i])) {
            NNH_UI_DROP(*b, s, "Ui::choice: the text bytes are full (UiDesc::text_bytes)");
            return {};
        }
    }
    x.index = static_cast<std::uint16_t>(d.index < 0 ? 0 : (d.index >= d.count ? d.count - 1 : d.index));
    x.wrap = d.wrap ? 1 : 0;
    return detail::handle_of(*b, s);
}

} // namespace nnh
