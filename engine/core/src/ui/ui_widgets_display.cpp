// Display widgets (spec 028 FR-5): label, image, progress bar, panel, spacer.

#include <cmath>

#include "ui/ui_block.hpp"
#include "ui/ui_widget_new.hpp"

namespace nnh {
namespace detail {

void reveal_track(UiBlock& b, int s); // ui_reveal.cpp

/// 16.16 characters per frame for a rate (< 0: no reveal, 0xFFFFFFFF marks "all shown"); false when not finite.
bool reveal_rate_bits(float rate, std::uint32_t& out) {
    if (!std::isfinite(rate)) return false;
    if (rate < 0) {
        out = 0;
        return true;
    }
    const float scaled = rate * 65536.0f + 0.5f;
    out = scaled >= 4294967040.0f ? 0xFFFFFF00u : static_cast<std::uint32_t>(scaled);
    return true;
}

} // namespace detail

UiWidget Ui::label(UiWidget parent, const UiLabelDesc& d) {
    detail::UiBlock* b = block_;
    if (b == nullptr) return {};
    std::uint32_t rate = 0;
    if (!detail::reveal_rate_bits(d.reveal_rate, rate)) {
        NNH_UI_MISUSE(*b, "Ui::label: a reveal rate that isn't finite");
        return {};
    }
    const int s = detail::widget_new(*b, parent, UiKind::Label, d.key, d.style, d.layout);
    if (s < 0) return {};
    detail::UiCold& c = b->cold[s];
    if (!detail::store_text(*b, d.text, c.text_kind, c.text)) {
        NNH_UI_DROP(*b, s, "Ui::label: the text bytes are full (UiDesc::text_bytes)");
        return {};
    }
    detail::UiLabelData& l = c.data.label;
    l = detail::UiLabelData{};
    l.max_lines = static_cast<std::uint8_t>(d.max_lines < 0 ? 0 : (d.max_lines > 255 ? 255 : d.max_lines));
    l.bits = static_cast<std::uint8_t>((d.wrap ? 1 : 0) | ((static_cast<unsigned>(d.align) & 3u) << 1) |
                                       ((static_cast<unsigned>(d.valign) & 3u) << 3));
    if (d.reveal_rate >= 0) {
        l.rate = rate;
        l.visible = 0;
        detail::reveal_track(*b, s);
    }
    return detail::handle_of(*b, s);
}

UiWidget Ui::image(UiWidget parent, const UiImageDesc& d) {
    detail::UiBlock* b = block_;
    if (b == nullptr) return {};
    if (d.texture < -1 || d.texture >= kMaxUiTextures) {
        NNH_UI_MISUSE(*b, "Ui::image: a texture slot outside -1..7");
        return {};
    }
    const int s = detail::widget_new(*b, parent, UiKind::Image, d.key, d.style, d.layout);
    if (s < 0) return {};
    detail::UiImageData& m = b->cold[s].data.image;
    m = detail::UiImageData{};
    m.texture = static_cast<std::int8_t>(d.texture);
    m.scale = static_cast<std::uint8_t>(d.scale < 1 ? 1 : (d.scale > 8 ? 8 : d.scale));
    m.rx = static_cast<std::int16_t>(d.region.x);
    m.ry = static_cast<std::int16_t>(d.region.y);
    m.rw = static_cast<std::int16_t>(d.region.w);
    m.rh = static_cast<std::int16_t>(d.region.h);
    m.tint = d.tint;
    return detail::handle_of(*b, s);
}

UiWidget Ui::progress(UiWidget parent, const UiProgressDesc& d) {
    detail::UiBlock* b = block_;
    if (b == nullptr) return {};
    if (d.max < 1) {
        NNH_UI_MISUSE(*b, "Ui::progress: max must be at least 1");
        return {};
    }
    const int s = detail::widget_new(*b, parent, UiKind::Progress, d.key, d.style, d.layout);
    if (s < 0) return {};
    detail::UiProgressData& p = b->cold[s].data.progress;
    p = detail::UiProgressData{};
    p.max = d.max;
    p.value = d.value < 0 ? 0 : (d.value > d.max ? d.max : d.value);
    p.bits = static_cast<std::uint8_t>((d.vertical ? 1 : 0) | (d.show_text ? 2 : 0));
    const int fn = detail::store_fn(*b, d.format);
    if (fn < 0) {
        NNH_UI_DROP(*b, s, "Ui::progress: the function slots are full (UiDesc::functions)");
        return {};
    }
    p.format = static_cast<std::uint8_t>(fn);
    return detail::handle_of(*b, s);
}

UiWidget Ui::panel(UiWidget parent, const UiPanelDesc& d) {
    detail::UiBlock* b = block_;
    if (b == nullptr) return {};
    if (d.as_box && d.box != UiKind::VBox && d.box != UiKind::HBox) {
        NNH_UI_MISUSE(*b, "Ui::panel: a panel's box is a VBox or an HBox");
        return {};
    }
    const int s = detail::widget_new(*b, parent, UiKind::Panel, d.key, d.style, d.layout);
    if (s < 0) return {};
    detail::UiBoxData& x = b->cold[s].data.box;
    x = detail::UiBoxData{};
    x.extra = static_cast<std::uint8_t>(!d.as_box ? 0 : (d.box == UiKind::VBox ? 1 : 2));
    x.spacing = static_cast<std::uint8_t>(d.spacing < 0 ? 0 : (d.spacing > 255 ? 255 : d.spacing));
    x.bits = 1; // wraps
    return detail::handle_of(*b, s);
}

UiWidget Ui::spacer(UiWidget parent, const UiLayout& layout) {
    detail::UiBlock* b = block_;
    if (b == nullptr) return {};
    const int s = detail::widget_new(*b, parent, UiKind::Spacer, UiKey{}, UiStyleId{}, layout);
    return s < 0 ? UiWidget{} : detail::handle_of(*b, s);
}

} // namespace nnh
