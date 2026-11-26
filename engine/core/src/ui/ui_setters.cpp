// Changing widgets (spec 028 FR-7, FR-4, FR-16, FR-17): visibility, enabled, values, style, layout, render offset and
// opacity (never a relayout), keys, neighbours, defaults, back widgets, callbacks. Each marks what it affects.

#include <cmath>

#include "ui/ui_block.hpp"
#include "ui/ui_focus.hpp"
#include "ui/ui_widget_new.hpp"

namespace nnh {
namespace {

/// The live slot of `w`, else a misuse report and -1.
int live(detail::UiBlock* b, UiWidget w, [[maybe_unused]] const char* who) {
    if (b == nullptr) return -1;
    const int s = detail::slot_of(*b, w);
    if (s < 0) NNH_UI_MISUSE(*b, who);
    return s;
}

/// True when the focus is `s` or inside its subtree.
bool holds_focus(const detail::UiBlock& b, int s) {
    for (int n = b.focus; n != detail::kUiNone; n = b.hot[n].parent) {
        if (n == s) return true;
    }
    return false;
}

void store_callback(detail::UiBlock& b, std::uint8_t& slot, const UiCallback& cb) {
    detail::fn_release(b, slot);
    slot = detail::kUiNoSlot;
    const int fn = detail::store_fn(b, cb);
    if (fn < 0) {
        NNH_UI_MISUSE(b, "Ui::on_activate/on_change: the function slots are full (UiDesc::functions)");
        return;
    }
    slot = static_cast<std::uint8_t>(fn);
}

} // namespace

void Ui::set_visible(UiWidget w, bool on) {
    const int s = live(block_, w, "Ui::set_visible: a stale or null widget");
    if (s < 0) return;
    detail::UiHot& n = block_->hot[s];
    if (((n.flags & detail::kUiVisible) != 0) == on) return;
    const bool repair = !on && holds_focus(*block_, s);
    const detail::RepairPoint at = detail::repair_point(*block_, s);
    n.flags = static_cast<std::uint8_t>(on ? n.flags | detail::kUiVisible : n.flags & ~detail::kUiVisible);
    detail::mark_dirty(*block_, s);
    if (repair) detail::set_focus(*block_, detail::repair_target(*block_, at), false);
}

void Ui::set_enabled(UiWidget w, bool on) {
    const int s = live(block_, w, "Ui::set_enabled: a stale or null widget");
    if (s < 0) return;
    detail::UiHot& n = block_->hot[s];
    if (((n.flags & detail::kUiEnabled) != 0) == on) return;
    const detail::RepairPoint at = detail::repair_point(*block_, s);
    n.flags = static_cast<std::uint8_t>(on ? n.flags | detail::kUiEnabled : n.flags & ~detail::kUiEnabled);
    // skipped when disabled (not inert): focus moves on, never staying on a widget that can't take it
    if (!on && holds_focus(*block_, s) && !detail::is_target(*block_, block_->focus)) {
        detail::set_focus(*block_, detail::repair_target(*block_, at), false);
    }
}

void Ui::set_value(UiWidget w, int v) {
    const int s = live(block_, w, "Ui::set_value: a stale or null widget");
    if (s >= 0) detail::value_set(*block_, s, v, false);
}

void Ui::set_progress_max(UiWidget w, int max) {
    const int s = live(block_, w, "Ui::set_progress_max: a stale or null widget");
    if (s < 0) return;
    if (detail::kind_of(*block_, s) != UiKind::Progress || max < 1) {
        NNH_UI_MISUSE(*block_, "Ui::set_progress_max: a progress bar and a max of at least 1");
        return;
    }
    detail::UiProgressData& p = block_->cold[s].data.progress;
    p.max = max;
    if (p.value > max) p.value = max;
}

void Ui::set_style(UiWidget w, UiStyleId id) {
    const int s = live(block_, w, "Ui::set_style: a stale or null widget");
    if (s < 0) return;
    if (id.value > block_->styles_used) {
        NNH_UI_MISUSE(*block_, "Ui::set_style: an unknown style");
        return;
    }
    block_->hot[s].style = id.value;
    detail::mark_dirty(*block_, s);
}

void Ui::set_layout(UiWidget w, const UiLayout& l) {
    const int s = live(block_, w, "Ui::set_layout: a stale or null widget");
    if (s >= 0) detail::apply_layout(*block_, s, l);
}

void Ui::set_render_offset(UiWidget w, Vec2 offset) {
    const int s = live(block_, w, "Ui::set_render_offset: a stale or null widget");
    if (s < 0) return;
    if (!std::isfinite(offset.x) || !std::isfinite(offset.y)) {
        NNH_UI_MISUSE(*block_, "Ui::set_render_offset: a value that isn't finite");
        return;
    }
    auto px = [](float v) {
        const float r = std::floor(v + 0.5f);
        return static_cast<std::int16_t>(r < -32768.0f ? -32768.0f : (r > 32767.0f ? 32767.0f : r));
    };
    block_->cold[s].offset_x = px(offset.x);
    block_->cold[s].offset_y = px(offset.y);
}

void Ui::set_opacity(UiWidget w, float opacity) {
    const int s = live(block_, w, "Ui::set_opacity: a stale or null widget");
    if (s < 0) return;
    if (!std::isfinite(opacity)) {
        NNH_UI_MISUSE(*block_, "Ui::set_opacity: a value that isn't finite");
        return;
    }
    const float o = opacity < 0 ? 0.0f : (opacity > 1 ? 1.0f : opacity);
    block_->cold[s].opacity = static_cast<std::uint8_t>(std::floor(o * 255.0f + 0.5f));
}

void Ui::set_key(UiWidget w, UiKey k) {
    const int s = live(block_, w, "Ui::set_key: a stale or null widget");
    if (s < 0) return;
    if (detail::key_taken(*block_, k.value, s)) {
        NNH_UI_MISUSE(*block_, "Ui::set_key: a key used twice");
        return;
    }
    block_->hot[s].key = k.value;
}

void Ui::set_neighbour(UiWidget w, UiDir d, UiWidget target) {
    const int s = live(block_, w, "Ui::set_neighbour: a stale or null widget");
    if (s < 0) return;
    const int t = target ? detail::slot_of(*block_, target) : -1;
    if (target && t < 0) {
        NNH_UI_MISUSE(*block_, "Ui::set_neighbour: a stale target");
        return;
    }
    block_->cold[s].neighbours[static_cast<int>(d) & 3] = t < 0 ? detail::kUiNone : static_cast<std::uint16_t>(t);
}

namespace {

/// `child` lies inside `container`'s subtree.
bool inside(const detail::UiBlock& b, int container, int child) {
    for (int n = b.hot[child].parent; n != detail::kUiNone; n = b.hot[n].parent) {
        if (n == container) return true;
    }
    return false;
}

void set_container_ref(detail::UiBlock* b, UiWidget container, UiWidget child, bool back, const char* who) {
    const int s = live(b, container, who);
    if (s < 0) return;
    const int c = child ? detail::slot_of(*b, child) : -1;
    if (!detail::is_container(detail::kind_of(*b, s)) || (child && (c < 0 || !inside(*b, s, c)))) {
        NNH_UI_MISUSE(*b, who);
        return;
    }
    std::uint16_t& ref = back ? b->cold[s].data.box.back : b->cold[s].data.box.def;
    ref = c < 0 ? detail::kUiNone : static_cast<std::uint16_t>(c);
}

} // namespace

void Ui::set_default(UiWidget container, UiWidget child) {
    set_container_ref(block_, container, child, false, "Ui::set_default: a container and a widget inside it");
}

void Ui::set_back(UiWidget container, UiWidget back) {
    set_container_ref(block_, container, back, true, "Ui::set_back: a container and a widget inside it");
}

void Ui::on_activate(UiWidget w, UiCallback cb) {
    const int s = live(block_, w, "Ui::on_activate: a stale or null widget");
    if (s >= 0) store_callback(*block_, block_->cold[s].on_activate, cb);
}

void Ui::on_change(UiWidget w, UiCallback cb) {
    const int s = live(block_, w, "Ui::on_change: a stale or null widget");
    if (s >= 0) store_callback(*block_, block_->cold[s].on_change, cb);
}

} // namespace nnh
