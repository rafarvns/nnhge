// The generic part of creating a widget (spec 028 FR-2, FR-7, FR-32): parent rules, keys, styles, capacity, the
// layout values. A refused creation asserts in Debug/Development and gives a null handle (one error log per UI in
// Release).

#include "ui/ui_block.hpp"
#include "ui/ui_widget_new.hpp"

namespace nnh::detail {
namespace {

std::int16_t clamp16(int v, int lo, int hi) { return static_cast<std::int16_t>(v < lo ? lo : (v > hi ? hi : v)); }

} // namespace

void apply_layout(UiBlock& b, int s, const UiLayout& l) {
    UiCold& c = b.cold[s];
    UiHot& n = b.hot[s];
    c.fixed_w = clamp16(l.width, 0, 32767);
    c.fixed_h = clamp16(l.height, 0, 32767);
    c.anchor = static_cast<std::uint8_t>(l.place.anchor);
    c.cflags = static_cast<std::uint8_t>((c.cflags & (kUiResolved | 0x18u)) | (l.place.fill_x ? kUiFillX : 0) |
                                         (l.place.fill_y ? kUiFillY : 0));
    c.px = clamp16(l.place.x, -32768, 32767);
    c.py = clamp16(l.place.y, -32768, 32767);
    c.pright = clamp16(l.place.right, -32768, 32767);
    c.pbottom = clamp16(l.place.bottom, -32768, 32767);
    n.align = static_cast<std::uint8_t>((static_cast<unsigned>(l.align) & 3u) | (l.expand ? kUiExpandBit : 0));
    n.stretch = static_cast<std::uint8_t>(l.stretch < 1 ? 1 : (l.stretch > 255 ? 255 : l.stretch));
    mark_dirty(b, s);
}

int widget_new(UiBlock& b, UiWidget parent, UiKind kind, UiKey key, UiStyleId style, const UiLayout& layout) {
    const int p = slot_of(b, parent);
    if (p < 0) {
        NNH_UI_MISUSE(b, "Ui: a widget under a stale or null parent");
        return -1;
    }
    const UiKind pk = kind_of(b, p);
    if (!is_container(pk)) {
        NNH_UI_MISUSE(b, "Ui: a widget under a widget that holds no children");
        return -1;
    }
    if (pk == UiKind::Panel && b.cold[p].data.box.extra == 0 && b.hot[p].first_child != kUiNone) {
        NNH_UI_MISUSE(b, "Ui: a panel holds one child (UiPanelDesc::as_box for more)");
        return -1;
    }
    if (key_taken(b, key.value, -1)) {
        NNH_UI_MISUSE(b, "Ui: a key used twice");
        return -1;
    }
    if (style.value > b.styles_used) {
        NNH_UI_MISUSE(b, "Ui: an unknown style (Ui::add_style)");
        return -1;
    }
    const int s = tree_take(b, kind, p);
    if (s < 0) {
        NNH_UI_MISUSE(b, "Ui: the UI is full (UiDesc::widgets)");
        return -1;
    }
    b.hot[s].key = key.value;
    b.hot[s].style = style.value;
    apply_layout(b, s, layout);
    return s;
}

void widget_drop(UiBlock& b, int s, const char* why) {
    tree_destroy(b, s);
    NNH_UI_MISUSE(b, why);
}

} // namespace nnh::detail
