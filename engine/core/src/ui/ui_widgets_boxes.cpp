// Containers and lists (spec 028 FR-9, FR-11, FR-12): vbox, hbox, grid box, stack, scroll box, list and grid (a slice
// of the row pool each).

#include "ui/ui_block.hpp"
#include "ui/ui_widget_new.hpp"

namespace nnh {
namespace {

std::uint8_t byte(int v) { return static_cast<std::uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v)); }

UiWidget box(detail::UiBlock* b, UiWidget parent, UiKind kind, const UiBoxDesc& d) {
    if (b == nullptr) return {};
    if (kind == UiKind::GridBox && d.columns < 1) {
        NNH_UI_MISUSE(*b, "Ui::grid_box: columns must be at least 1");
        return {};
    }
    const int s = detail::widget_new(*b, parent, kind, d.key, d.style, d.layout);
    if (s < 0) return {};
    detail::UiBoxData& x = b->cold[s].data.box;
    x = detail::UiBoxData{};
    x.padding = byte(d.padding);
    x.spacing = byte(d.spacing);
    x.extra = kind == UiKind::GridBox ? byte(d.columns) : 0;
    x.bits = static_cast<std::uint8_t>((d.wrap ? 1 : 0) | (d.wrap_on_repeat ? 2 : 0) | (d.disabled_focusable ? 4 : 0));
    return detail::handle_of(*b, s);
}

} // namespace

UiWidget Ui::vbox(UiWidget parent, const UiBoxDesc& d) { return box(block_, parent, UiKind::VBox, d); }
UiWidget Ui::hbox(UiWidget parent, const UiBoxDesc& d) { return box(block_, parent, UiKind::HBox, d); }
UiWidget Ui::grid_box(UiWidget parent, const UiBoxDesc& d) { return box(block_, parent, UiKind::GridBox, d); }
UiWidget Ui::stack(UiWidget parent, const UiBoxDesc& d) { return box(block_, parent, UiKind::Stack, d); }

UiWidget Ui::scroll_box(UiWidget parent, const UiScrollDesc& d) {
    detail::UiBlock* b = block_;
    if (b == nullptr) return {};
    const int s = detail::widget_new(*b, parent, UiKind::ScrollBox, d.key, d.style, d.layout);
    if (s < 0) return {};
    detail::UiBoxData& x = b->cold[s].data.box;
    x = detail::UiBoxData{};
    x.padding = byte(d.padding);
    x.spacing = byte(d.spacing);
    x.extra = byte(d.margin);
    x.bits = 0; // a scroll box doesn't wrap
    return detail::handle_of(*b, s);
}

UiWidget Ui::list(UiWidget parent, const UiListDesc& d) {
    detail::UiBlock* b = block_;
    if (b == nullptr) return {};
    if (!d.fill) {
        NNH_UI_MISUSE(*b, "Ui::list: a list needs a fill function");
        return {};
    }
    if (d.rows < 1 || d.rows > 255 || d.columns < 1 || d.columns > 255 || d.count < 0 || d.count > kMaxUiItems) {
        NNH_UI_MISUSE(*b, "Ui::list: rows and columns 1..255, count 0..65535");
        return {};
    }
    if (b->rows_used + d.rows * d.columns > b->rows_capacity) {
        NNH_UI_MISUSE(*b, "Ui::list: the row pool is full (UiDesc::rows)");
        return {};
    }
    const UiKind kind = d.columns > 1 ? UiKind::Grid : UiKind::List;
    const int s = detail::widget_new(*b, parent, kind, d.key, d.style, d.layout);
    if (s < 0) return {};
    detail::UiListData& x = b->cold[s].data.list;
    x = detail::UiListData{};
    const int fn = detail::store_fn(*b, d.fill);
    if (fn < 0) {
        NNH_UI_DROP(*b, s, "Ui::list: the function slots are full (UiDesc::functions)");
        return {};
    }
    x.fill = static_cast<std::uint8_t>(fn);
    x.first_row = static_cast<std::uint16_t>(detail::rows_take(*b, d.rows * d.columns));
    x.rows = static_cast<std::uint8_t>(d.rows);
    x.columns = static_cast<std::uint8_t>(d.columns);
    x.count = static_cast<std::uint16_t>(d.count);
    x.margin = byte(d.margin);
    x.row_height = byte(d.row_height);
    x.bits = static_cast<std::uint8_t>((d.wrap ? 1 : 0) | (d.tap_selects_first ? 2 : 0));
    x.cell_width = static_cast<std::int16_t>(d.cell_width < 0 ? 0 : (d.cell_width > 32767 ? 32767 : d.cell_width));
    return detail::handle_of(*b, s);
}

} // namespace nnh
