// update() (spec 028 FR-20 to FR-22, plan "Update order"): (1) activation — the first update, after reset_input(), a
// frame_count() gap or a modal opening: every control already held is ignored until released; (2) the input; (3) the
// suppression mask; (4) repeat; (5) results cleared, the pressed look counted down, reveals advanced; (6) touch;
// (7) the direction; (8) pages; (9) confirm; (10) cancel; (11, 12) results sorted, layout, callbacks, opt-in events,
// layout; (13) the cursor.

#include "nnh/build.hpp"
#include "reload/reload_hooks.hpp"
#include "nnh/app.hpp"
#include "nnh/profile.hpp"
#include "ui/ui_block.hpp"
#include "ui/ui_focus.hpp"

namespace nnh::detail {
namespace {

void activation(UiBlock& b, const UiInput& in) {
    const std::uint64_t now = frame_count();
    const bool gap = b.active && now != b.last_frame && now != b.last_frame + 1;
    b.last_frame = now;
    if (b.active && !gap && !b.activate_pending) return;
    b.active = true;
    b.activate_pending = false;
    b.suppressed = in.held;
    b.touch_suppressed = in.touching;
    b.repeat = RepeatState{};
    touch_reset(b);
    if (b.focus == kUiNone) { // the UI becomes active: focus enters its first root (FR-17)
        layout_if_dirty(b);
        set_focus(b, enter_target(b, b.modal_depth > 0 ? b.modals[b.modal_depth - 1].container
                                                     : b.roots[screen_index(b.first_focus)]),
                  false);
    }
}

UiInput suppress(UiBlock& b, const UiInput& given) {
    b.suppressed &= given.held;
    if (!given.touching && !given.touch_ended) b.touch_suppressed = false;
    UiInput in = given;
    in.held = static_cast<std::uint8_t>(in.held & ~b.suppressed);
    in.pressed = static_cast<std::uint8_t>(in.pressed & ~b.suppressed & in.held);
    in.repeated = static_cast<std::uint8_t>(in.repeated & ~b.suppressed);
    if (b.touch_suppressed) {
        in.touching = in.touch_started = in.touch_ended = false;
        if (!given.touching) b.touch_suppressed = false; // its release ends the suppression
    }
    return in;
}

void cancel(UiBlock& b) {
    const int f = b.focus;
    const int scope = b.modal_depth > 0 ? b.modals[b.modal_depth - 1].container : -1;
    // the innermost container (from the focus up to the modal) naming a back widget
    for (int n = f == kUiNone ? scope : b.hot[f].parent; n >= 0 && n != kUiNone; n = b.hot[n].parent) {
        if (is_container(kind_of(b, n)) && b.cold[n].data.box.back != kUiNone) {
            const int back = b.cold[n].data.box.back;
            add_result(b, UiResultKind::Cancelled, f == kUiNone ? n : f);
            activate(b, back);
            return;
        }
        if (n == scope) break;
    }
    add_result(b, UiResultKind::Cancelled, f == kUiNone ? (scope >= 0 ? scope : -1) : f);
    if (b.modal_depth > 0 && b.modals[b.modal_depth - 1].cancel_closes) {
        modal_close(b);
    }
}

void step(UiBlock& b, const UiInput& given) {
    NNH_PROFILE_SCOPE("ui update");
    activation(b, given);
    const UiInput in = suppress(b, given);
    const RepeatFired fired = repeat_step(b.repeat, in.held, in.pressed, in.repeated, b.input.repeat_delay,
                                          b.input.repeat_interval);
    b.result_count = 0;
    if (b.pressed_frames > 0 && --b.pressed_frames == 0) b.pressed_slot = kUiNone;
    reveal_step(b);
    layout_if_dirty(b);
    touch_step(b, in);
    for (int d = 0; d < 4; ++d) {
        const auto bit = static_cast<std::uint8_t>(1u << d);
        if (((fired.fresh | fired.repeat) & bit) == 0) continue;
        if (b.input.hide_cursor_after_touch) b.cursor.hidden = false;
        navigate(b, static_cast<UiDir>(d), (fired.fresh & bit) != 0);
    }
    for (int page = 0; page < 2; ++page) {
        const auto bit = ui_bit(page == 0 ? UiControl::PageBack : UiControl::PageForward);
        if (((fired.fresh | fired.repeat) & bit) == 0 || b.focus == kUiNone) continue;
        const int f = b.focus;
        if (is_list(kind_of(b, f))) list_page(b, f, page == 0 ? -1 : 1);
        add_result(b, UiResultKind::Page, f, page == 0 ? -1 : 1,
                   is_list(kind_of(b, f)) ? b.cold[f].data.list.first : -1);
    }
    if ((fired.fresh & ui_bit(UiControl::Confirm)) != 0) {
        if (b.focus == kUiNone) {
            navigate(b, UiDir::Down, true); // enters the first root
        } else {
            activate(b, b.focus, is_list(kind_of(b, b.focus)) ? b.cold[b.focus].data.list.focused : -1);
        }
    }
    if ((fired.fresh & ui_bit(UiControl::Cancel)) != 0) cancel(b);
    finish_results(b);
    if (b.orphaned) return;
    cursor_step(b);
}

} // namespace

void ui_step(UiBlock& b, const UiInput& in) {
    if constexpr (build::kDevToolsEnabled) { // spec 031: a hot reload this frame (strings, fonts, textures) relays out
        const std::uint32_t frame = detail::g_reload_applied_frame; // a hook: no reload code linked for it
        if (frame != 0 && frame == static_cast<std::uint32_t>(frame_count())) forget_text(b);
    }
    step(b, in);
}

void ui_read_input(UiBlock& b, UiInput& in) {
    if (b.source) {
        b.source(in);
    } else if (b.adapter != nullptr) {
        b.adapter(b, in);
    } else {
        read_raw_input(b, in);
    }
}

void ui_reset_input(UiBlock& b) {
    b.active = false;
    b.repeat = RepeatState{};
    b.pressed_slot = kUiNone;
    b.pressed_frames = 0;
    touch_reset(b);
}

} // namespace nnh::detail

namespace nnh {

void Ui::update() {
    detail::UiBlock* b = block_;
    if (b == nullptr) return;
    UiInput in;
    detail::ui_read_input(*b, in);
    update(in);
}

void Ui::update(const UiInput& input) {
    detail::UiBlock* b = block_;
    if (b == nullptr) return;
    if (b->busy > 0) {
        NNH_UI_MISUSE(*b, "Ui::update from the UI's own callback");
        return;
    }
    if (b->scene != nullptr) {
        NNH_UI_MISUSE(*b, "Ui::update on a UI bound to a scene (the binding updates it)");
        return;
    }
    detail::ui_step(*b, input);
    if (b->orphaned) detail::ui_free(*b); // destroyed from its own callback (Release): `this` may be gone too
}

void Ui::set_input_source(UiInputSource source) {
    if (block_ == nullptr) return;
    block_->source = source;
}

void Ui::reset_input() {
    detail::UiBlock* b = block_;
    if (b == nullptr) return;
    detail::ui_reset_input(*b);
}

} // namespace nnh
