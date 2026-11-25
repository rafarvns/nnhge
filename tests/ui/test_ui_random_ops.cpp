// Random operations (spec 028 T6.7, FR-32, FR-15): 100 000 seeded operations — creating, destroying, hiding, disabling,
// setting text and values, focusing, modals, input with touches, drawing — keeping the invariants: links consistent,
// the live count right, at most one focus and it a target, modals open on live containers. Run under ASan/UBSan on the
// Linux build VM.

#include "ui/ui_block.hpp"
#include "ui/ui_focus.hpp"
#include "ui_fixture.hpp"

using nnh::Screen;
using nnh::Ui;
using nnh::UiInput;
using nnh::UiWidget;
using nnh::detail::kUiFreeKind;
using nnh::detail::kUiNone;
using nnh::detail::UiBlock;

namespace {

std::uint32_t g_seed = 12345;
std::uint32_t next() {
    g_seed ^= g_seed << 13;
    g_seed ^= g_seed >> 17;
    g_seed ^= g_seed << 5;
    return g_seed;
}

bool consistent(const UiBlock& b) {
    int live = 0;
    for (int s = 0; s < b.capacity; ++s) {
        const auto& n = b.hot[s];
        if (n.kind == kUiFreeKind) continue;
        ++live;
        if (n.parent != kUiNone) {
            if (b.hot[n.parent].kind == kUiFreeKind) return false;
            bool found = false;
            for (int c = b.hot[n.parent].first_child; c != kUiNone; c = b.hot[c].next) found = found || c == s;
            if (!found) return false;
        }
        int count = 0, last = kUiNone;
        for (int c = n.first_child; c != kUiNone; c = b.hot[c].next) {
            if (b.hot[c].parent != s || ++count > b.capacity) return false;
            last = c;
        }
        if (n.first_child != kUiNone && b.hot[n.first_child].prev != last) return false;
    }
    if (live != b.live) return false;
    if (b.focus != kUiNone && (b.hot[b.focus].kind == kUiFreeKind)) return false;
    for (int i = 0; i < b.modal_depth; ++i) {
        if (b.hot[b.modals[i].container].kind == kUiFreeKind) return false;
    }
    return b.text_used <= b.text_capacity && b.rows_used <= b.rows_capacity && b.texts_used <= b.texts_capacity;
}

UiWidget pick(const Ui& ui, const UiBlock& b) {
    for (int tries = 0; tries < 8; ++tries) {
        const int s = static_cast<int>(next() % static_cast<std::uint32_t>(b.capacity));
        if (b.hot[s].kind != kUiFreeKind) return nnh::detail::handle_of(b, s);
    }
    return ui.root(next() % 2 == 0 ? Screen::Top : Screen::Bottom);
}

} // namespace

NNH_TEST(ui_random_operations_keep_the_invariants) {
    ui_test::restart();
    {
        Ui ui = ui_test::make({.widgets = 96, .text_bytes = 2048, .texts = 64, .rows = 64, .functions = 32});
        const UiBlock& b = *nnh::UiAccess::block(ui);
        static const nnh::UiText opts[3] = {"x", "yy", "zzz"};
        bool ok = true;
        int focus_bad = 0;
        for (int i = 0; i < 100000 && ok; ++i) {
            const UiWidget w = pick(ui, b);
            const bool container = nnh::detail::is_container(ui.kind(w));
            const bool full = ui.stats().widgets > 88 || b.rows_used > 56 || b.texts_used > 56 || b.text_used > 1900 ||
                              b.fns_used > 28;
            switch (next() % 20) {
            case 0: if (container && !full) ui.vbox(w); break;
            case 1: if (container && !full) ui.hbox(w); break;
            case 2: if (container && !full) ui.button(w, {.text = "button"}); break;
            case 3: if (container && !full) ui.slider(w, {.text = "s", .value = 3}); break;
            case 4: if (container && !full) ui.choice(w, {.text = "c", .options = opts, .count = 3}); break;
            case 5: if (container && !full) ui.list(w, {.rows = 3, .count = static_cast<int>(next() % 50), .fill = [](int, nnh::UiRow& r) { r.enabled = r.number != 0; }}); break;
            case 6: if (container && !full) ui.scroll_box(w, {.layout = {.height = 40}}); break;
            case 7: if (ui.kind(w) != nnh::UiKind::Root) ui.destroy(w); break;
            case 8: ui.set_visible(w, next() % 3 != 0); break;
            case 9: ui.set_enabled(w, next() % 3 != 0); break;
            case 10: ui.set_text(w, next() % 2 == 0 ? nnh::UiText("text") : nnh::UiText::borrow("borrowed")); break;
            case 11: ui.focus(w); break;
            case 12:
                if (container && ui.kind(w) != nnh::UiKind::Root && ui.modal_depth() < 8) ui.open_modal(w);
                else if (ui.modal_depth() > 0) ui.close_modal();
                break;
            case 13: ui.update(ui_test::press(static_cast<std::uint8_t>(next() & 0xFF))); break;
            case 14: ui.update(ui_test::hold(static_cast<std::uint8_t>(next() & 0x0F))); break;
            case 15: ui.update(ui_test::touch_start(static_cast<int>(next() % 320), static_cast<int>(next() % 240))); break;
            case 16: ui.update(ui_test::touch_hold(static_cast<int>(next() % 320), static_cast<int>(next() % 240))); break;
            case 17: ui.update(ui_test::touch_end(static_cast<int>(next() % 320), static_cast<int>(next() % 240))); break;
            case 18:
                if (nnh::detail::is_list(ui.kind(w))) ui.set_item_count(w, static_cast<int>(next() % 40));
                else if (ui.kind(w) == nnh::UiKind::ScrollBox) ui.set_scroll(w, static_cast<int>(next() % 5));
                break;
            default:
                if (i % 50 == 0) {
                    CHECK(nnh::frame());
                    ui.draw();
                }
                break;
            }
            ok = consistent(b);
            // the focus is none or a target (a scroll box's window aside: the game may scroll the focus away)
            UiBlock& mb = *nnh::UiAccess::block(ui);
            mb.ignore_window = true;
            if (b.focus != kUiNone && !nnh::detail::is_target(b, b.focus)) ++focus_bad;
            mb.ignore_window = false;
        }
        CHECK(ok);
        CHECK(focus_bad == 0);
    }
    nnh::shutdown();
}
