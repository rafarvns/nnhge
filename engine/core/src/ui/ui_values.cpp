// Values (spec 028 FR-6, FR-16): sliders take left/right (step, clamped; at a limit: blocked), choices take left/right
// (wrapping per widget; at a non-wrapping end: blocked) and step on confirm, toggles flip on confirm and take left
// (off) / right (on) only when that changes them. Activation: buttons and lists report Activated (a list with its
// item), disabled-but-inert widgets and disabled rows report Refused. set_value reports nothing.

#include "ui/ui_focus.hpp"

namespace nnh::detail {
namespace {

void press_look(UiBlock& b, int s) {
    b.pressed_slot = static_cast<std::uint16_t>(s);
    b.pressed_frames = kUiPressedFrames;
}

/// Writes a value; reports ValueChanged when `report` and it changed. @return it changed.
bool write(UiBlock& b, int s, int v, bool report) {
    UiKindData& d = b.cold[s].data;
    int old = 0, now = 0;
    switch (kind_of(b, s)) {
    case UiKind::Slider:
        old = d.slider.value;
        d.slider.value = now = v < d.slider.min ? d.slider.min : (v > d.slider.max ? d.slider.max : v);
        break;
    case UiKind::Progress:
        old = d.progress.value;
        d.progress.value = now = v < 0 ? 0 : (v > d.progress.max ? d.progress.max : v);
        break;
    case UiKind::Toggle:
        old = d.toggle.checked;
        d.toggle.checked = static_cast<std::uint8_t>(now = v != 0 ? 1 : 0);
        break;
    case UiKind::Choice:
        old = d.choice.index;
        now = v < 0 ? 0 : (v >= d.choice.count ? d.choice.count - 1 : v);
        d.choice.index = static_cast<std::uint16_t>(now);
        break;
    case UiKind::List:
    case UiKind::Grid: list_set_item(b, s, v); return false;
    default: NNH_UI_MISUSE(b, "Ui::set_value: a widget without a value"); return false;
    }
    if (old == now) return false;
    if (report) add_result(b, UiResultKind::ValueChanged, s, now);
    return true;
}

} // namespace

void value_set(UiBlock& b, int s, int v, bool report) { write(b, s, v, report); }

bool value_direction(UiBlock& b, int s, UiDir d, bool fresh) {
    (void)fresh;
    if (d == UiDir::Up || d == UiDir::Down) return false;
    const int sign = d == UiDir::Right ? 1 : -1;
    UiKindData& x = b.cold[s].data;
    if (inert(b, s)) return false; // inert widgets don't change: the direction moves on
    switch (kind_of(b, s)) {
    case UiKind::Slider:
        if (!write(b, s, x.slider.value + sign * x.slider.step, true)) add_result(b, UiResultKind::Blocked, s);
        return true;
    case UiKind::Choice: {
        int i = x.choice.index + sign;
        if (i < 0 || i >= x.choice.count) {
            if (x.choice.wrap == 0) {
                add_result(b, UiResultKind::Blocked, s);
                return true;
            }
            i = i < 0 ? x.choice.count - 1 : 0;
        }
        write(b, s, i, true);
        return true;
    }
    case UiKind::Toggle:
        if (x.toggle.left_right == 0 || (sign > 0) == (x.toggle.checked != 0)) return false;
        write(b, s, sign > 0 ? 1 : 0, true);
        return true;
    default: return false;
    }
}

void activate(UiBlock& b, int s, int index) {
    if (s < 0 || b.hot[s].kind == kUiFreeKind) return;
    if (inert(b, s)) {
        add_result(b, UiResultKind::Refused, s, 0, index);
        return;
    }
    press_look(b, s);
    UiKindData& x = b.cold[s].data;
    switch (kind_of(b, s)) {
    case UiKind::Toggle: write(b, s, x.toggle.checked != 0 ? 0 : 1, true); break;
    case UiKind::Choice: write(b, s, x.choice.index + 1 >= x.choice.count ? 0 : x.choice.index + 1, true); break;
    case UiKind::List:
    case UiKind::Grid:
        if (index < 0 || index >= x.list.count) return;
        if (!list_row_enabled(b, s, index)) {
            add_result(b, UiResultKind::Refused, s, 0, index);
            return;
        }
        add_result(b, UiResultKind::Activated, s, 0, index);
        break;
    default: add_result(b, UiResultKind::Activated, s, 0, index); break;
    }
}

} // namespace nnh::detail
