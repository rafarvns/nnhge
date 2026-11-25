// Debugging (spec 028 FR-30): a dump of the tree to the log (kind, key, rectangle, state, value, one line per widget)
// and a debug draw of the visible widgets' rectangles with the focus path highlighted, on layer 15. Both compile to
// nothing in Release (no text kept).

#include "nnh/log.hpp"
#include "ui/ui_draw_out.hpp"
#include "ui/ui_focus.hpp"

NNH_LOG_CHANNEL_EXTERN(ui);

namespace nnh {

#if !defined(NNH_FLAVOR_RELEASE)
namespace {

const char* kind_name(UiKind k) {
    static const char* const kNames[] = {"root",   "label",  "image", "progress", "panel", "spacer",
                                         "button", "toggle", "slider", "choice",  "list",  "grid",
                                         "vbox",   "hbox",   "gridbox", "stack",  "scrollbox"};
    const auto i = static_cast<unsigned>(k);
    return i < sizeof kNames / sizeof kNames[0] ? kNames[i] : "?";
}

const char* state_name(UiState s) {
    static const char* const kNames[] = {"normal", "focused", "pressed", "disabled", "checked"};
    return kNames[static_cast<unsigned>(s) % 5u];
}

int depth_of(const detail::UiBlock& b, int s) {
    int d = 0;
    for (int n = b.hot[s].parent; n != detail::kUiNone; n = b.hot[n].parent) ++d;
    return d;
}

} // namespace
#endif

void Ui::dump() const {
#if !defined(NNH_FLAVOR_RELEASE)
    detail::UiBlock* b = block_;
    if (b == nullptr) return;
    detail::layout_if_dirty(*b);
    for (int i = 0; i < 2; ++i) {
        const int root = b->roots[i];
        if (root == detail::kUiNone) continue;
        for (int s = root; s >= 0; s = detail::preorder_next(*b, root, s)) {
            const detail::UiHot& n = b->hot[s];
            const int key = n.key == 0xFFFF ? -1 : n.key;
            NNH_LOG_INFO(ui, "ui dump: %*s%s key=%d rect=%d,%d,%d,%d state=%s value=%d%s", 2 * depth_of(*b, s), "",
                         kind_name(detail::kind_of(*b, s)), key, n.x, n.y, n.w, n.h,
                         state_name(detail::state_of(*b, s)), detail::value_of(*b, s),
                         (n.flags & detail::kUiVisible) == 0 ? " hidden" : "");
        }
    }
#endif
}

void Ui::debug_draw(Screen screen) const {
#if !defined(NNH_FLAVOR_RELEASE)
    detail::UiBlock* b = block_;
    if (b == nullptr) return;
    const int root = b->roots[detail::screen_index(screen)];
    if (root == detail::kUiNone) return;
    detail::layout_if_dirty(*b);
    const std::uint32_t hash = b->draw_hash; // the debug draw doesn't count as the UI's draws
    const int draws = b->draws;
    detail::DrawCtx d;
    d.screen = screen;
    d.layer = 15;
    for (int s = root; s >= 0; s = detail::preorder_next_visible(*b, root, s)) {
        bool on_path = false;
        for (int n = b->focus; n != detail::kUiNone && !on_path; n = b->hot[n].parent) on_path = n == s;
        detail::out_outline(*b, d, detail::rect_of(b->hot[s]), on_path ? rgb(255, 220, 0) : rgba(0, 220, 255, 160));
    }
    b->draw_hash = hash;
    b->draws = draws;
#else
    (void)screen;
#endif
}

} // namespace nnh
