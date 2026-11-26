// The live panel's touch input (spec 030 FR-9; plan "Panel", touch): a tap selects a row (a group row toggles), a touch
// on a numeric row's bar sets the value under it every tick, `-`/`+` step by one and repeat while held (24/6 ticks), a
// tap on a bool's value toggles it, `run` runs an action. A touch that starts outside the rows does nothing until it
// ends. Pure over the registry. Tests: tests/live/test_live_panel_touch.cpp.

#include "debug/dev_controls.hpp"
#include "live/live_apply.hpp"
#include "live/live_panel_layout.hpp"
#include "live/live_panel_model.hpp"
#include "live/live_steps.hpp"

namespace nnh::detail::live {
namespace {

enum Mode : std::uint8_t { kNone, kIgnored, kDrag, kMinus, kPlus };

bool numeric(const Entry& e) { return e.kind == Kind::Float || e.kind == Kind::Int32; }

std::uint32_t drag_bits(const Entry& e, int x, int depth) {
    if (e.kind == Kind::Float) {
        FloatGrid g;
        if (make_float_grid(bits_float(e.min), bits_float(e.max), bits_float(e.step), g) != RangeError::None) return e.def;
        return float_bits(drag_value(g, x, bar_x0(depth), bar_w(depth)));
    }
    IntGrid g;
    if (make_int_grid(static_cast<std::int32_t>(e.min), static_cast<std::int32_t>(e.max), static_cast<std::int32_t>(e.step),
                      g) != RangeError::None) {
        return e.def;
    }
    return static_cast<std::uint32_t>(drag_value(g, x, bar_x0(depth), bar_w(depth)));
}

void step_button(Registry& r, int slot, int delta) {
    Entry& e = r.entries[slot];
    set_pending(r, slot, stepped(e, edit_base(e), delta, r.panel.channel));
}

void start(Registry& r, const DevInput& in) {
    PanelState& p = r.panel;
    const int visible = row_under(in.touch_y, kRowHeight, kTunableRowsVisible);
    Row row;
    if (visible < 0 || !row_at(r, kPageTunables, p.scroll + visible, row)) {
        p.touch_mode = kIgnored;
        return;
    }
    if (p.selected != p.scroll + visible) p.channel = 0;
    p.selected = p.scroll + visible;
    p.touch_mode = kNone;
    if (row.type == RowType::Group) {
        toggle_group(p, fnv1a32(row.name, row.length));
        return;
    }
    Entry& e = r.entries[row.ref];
    const int y = in.touch_y - row_y(visible, kRowHeight);
    const int x = in.touch_x;
    p.touch_slot = row.ref;
    if (e.kind == Kind::Action) {
        if (x >= kMinusX) request_run(r, row.ref);
        return;
    }
    if (x >= kMinusX && x < kMinusX + kMinusW) {
        p.touch_mode = kMinus;
        p.touch_ticks = 0;
        step_button(r, row.ref, -1);
    } else if (x >= kPlusX && x < kPlusX + kPlusW) {
        p.touch_mode = kPlus;
        p.touch_ticks = 0;
        step_button(r, row.ref, 1);
    } else if (numeric(e) && y >= kLine2Dy && x <= kBarRight) {
        p.touch_mode = kDrag;
        set_pending(r, row.ref, drag_bits(e, x, row.depth));
    } else if (e.kind == Kind::Bool && y < kLine2Dy && x > kBarRight) {
        set_pending(r, row.ref, edit_base(e) != 0 ? 0u : 1u);
    }
}

void held(Registry& r, const DevInput& in) {
    PanelState& p = r.panel;
    if (p.touch_mode == kDrag) {
        Row row;
        if (!row_at(r, kPageTunables, p.selected, row) || row.type != RowType::Tunable || row.ref != p.touch_slot) return;
        set_pending(r, row.ref, drag_bits(r.entries[row.ref], in.touch_x, row.depth));
        return;
    }
    if (p.touch_mode != kMinus && p.touch_mode != kPlus) return;
    const int t = ++p.touch_ticks;
    if (t >= kDevRepeatDelay && (t - kDevRepeatDelay) % kDevRepeatEvery == 0) {
        Row row;
        if (row_at(r, kPageTunables, p.selected, row) && row.type == RowType::Tunable && row.ref == p.touch_slot) {
            step_button(r, row.ref, p.touch_mode == kMinus ? -1 : 1);
        }
    }
}

} // namespace

void panel_touch(Registry& r, const DevInput& in) {
    if (in.touch_started) start(r, in);
    else if (in.touching) held(r, in);
    if (in.touch_ended || !in.touching) r.panel.touch_mode = kNone;
}

} // namespace nnh::detail::live
