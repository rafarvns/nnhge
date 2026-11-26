// Creation checks (ui_desc_check.hpp).

#include "ui/ui_desc_check.hpp"

namespace nnh::detail {
namespace {

bool one_button(Button b) {
    const auto v = static_cast<std::uint32_t>(b);
    return v != 0 && (v & (v - 1)) == 0;
}

bool in(int v, int lo, int hi) { return v >= lo && v <= hi; }

Error bad(const char* what) { return Error::make(ErrorCode::InvalidArgument, "ui: %s", what); }

Error check_input(const UiInputConfig& in_cfg) {
    if (!in(in_cfg.repeat_delay, 0, 255)) return bad("repeat_delay must be 0..255");
    if (!in(in_cfg.repeat_interval, 1, 255)) return bad("repeat_interval must be 1..255");
    const float p = in_cfg.circle_press, r = in_cfg.circle_release;
    // written so that NaN fails every comparison
    if (!(r > 0.0f && r <= p && p <= 1.0f)) return bad("circle thresholds: 0 < circle_release <= circle_press <= 1");
    if (!one_button(in_cfg.confirm) || !one_button(in_cfg.cancel)) return bad("confirm and cancel: one button each");
    if (!one_button(in_cfg.page_back) || !one_button(in_cfg.page_forward)) return bad("page buttons: one button each");
    const Button all[4] = {in_cfg.confirm, in_cfg.cancel, in_cfg.page_back, in_cfg.page_forward};
    for (int i = 0; i < 4; ++i)
        for (int j = i + 1; j < 4; ++j)
            if (all[i] == all[j]) return bad(i < 2 ? "confirm, cancel and page buttons must differ"
                                                   : "page buttons must differ");
    return Error{ErrorCode::None, {}};
}

} // namespace

Error check_ui_desc(const UiDesc& d) {
    const int roots = (d.top ? 1 : 0) + (d.bottom ? 1 : 0);
    if (!in(d.widgets, 1, kMaxUiWidgets)) return bad("widgets must be 1..4096");
    if (roots == 0) return bad("a UI needs a root: top or bottom");
    if (d.widgets < roots) return bad("widgets must count the roots");
    if (!in(d.text_bytes, 0, 65535)) return bad("text_bytes must be 0..65535");
    if (!in(d.texts, 0, 4096)) return bad("texts must be 0..4096");
    if (!in(d.styles, 0, 240)) return bad("styles must be 0..240");
    if (!in(d.rows, 0, 1024)) return bad("rows must be 0..1024");
    if (!in(d.functions, 0, 255)) return bad("functions must be 0..255");
    if (!in(d.results, 8, 256)) return bad("results must be 8..256");
    if ((d.first_focus == Screen::Top && !d.top) || (d.first_focus == Screen::Bottom && !d.bottom)) {
        return bad("first_focus names a screen without a root");
    }
    return check_input(d.input);
}

} // namespace nnh::detail
