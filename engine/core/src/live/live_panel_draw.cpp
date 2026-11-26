// The Tunables and Watches pages (spec 030 FR-7, FR-8; plan "Panel", layout): drawn into the debug pass with the
// built-in font and overlay rectangles, integer positions only, so the pages are the same pixels everywhere.
// Tests: tests/live/test_live_panel_draw.cpp.

#include <cstdio>
#include <cstring>

#include "debug/dev_tools.hpp"
#include "live/live_apply.hpp"
#include "live/live_format.hpp"
#include "live/live_panel_layout.hpp"
#include "live/live_panel_model.hpp"
#include "live/live_watch.hpp"

namespace nnh::detail::live {
namespace {

constexpr Screen kBottom = Screen::Bottom;
constexpr Color kWhite{255, 255, 255, 255};
constexpr Color kGray{150, 150, 160, 255};
constexpr Color kYellow{255, 220, 60, 255};
constexpr Color kCyan{90, 220, 240, 255};
constexpr Color kSelected{50, 80, 150, 210};
constexpr Color kButton{70, 70, 90, 230};
constexpr Color kBarBack{60, 60, 70, 230};
constexpr Color kBarFill{90, 200, 120, 255};

void text(int x, int y, const char* t, Color c) { debug_record_text(kBottom, x, y, t, c); }
void rect(int x, int y, int w, int h, Color c) { debug_record_rect(kBottom, x, y, w, h, c); }
int width(const char* t) { return static_cast<int>(std::strlen(t)) * kCell; }

/// The text of an entry's bits as the panel shows it (short colours, enums `< name >`).
void value_text(const Entry& e, std::uint32_t bits, char* out, int size) {
    if (e.kind == Kind::Enum) {
        const char* n = bits < e.enum_count && e.enum_names != nullptr ? e.enum_names[bits] : "?";
        std::snprintf(out, static_cast<std::size_t>(size), "< %s >", n);
        return;
    }
    char v[kValueTextBytes];
    format_bits(e.kind, bits, true, v);
    std::snprintf(out, static_cast<std::size_t>(size), "%s", v);
}

void range_text(const Entry& e, char* out) {
    char a[kValueTextBytes], b[kValueTextBytes], t[2 * kValueTextBytes + 4];
    format_bits(e.kind, e.min, true, a);
    format_bits(e.kind, e.max, true, b);
    std::snprintf(t, sizeof t, "%s..%s", a, b);
    int n = 0;
    while (n < 10 && t[n] != '\0') { // cut to 10 characters
        out[n] = t[n];
        ++n;
    }
    out[n] = '\0';
}

void draw_bar(const Entry& e, std::uint32_t bits, int y, int depth) {
    const int x0 = bar_x0(depth), w = bar_w(depth);
    int fill = 0;
    if (e.kind == Kind::Float) {
        FloatGrid g;
        if (make_float_grid(bits_float(e.min), bits_float(e.max), bits_float(e.step), g) == RangeError::None) {
            fill = bar_fill(g, bits_float(bits), w);
        }
    } else {
        IntGrid g;
        if (make_int_grid(static_cast<std::int32_t>(e.min), static_cast<std::int32_t>(e.max),
                          static_cast<std::int32_t>(e.step), g) == RangeError::None) {
            fill = bar_fill(g, static_cast<std::int32_t>(bits), w);
        }
    }
    rect(x0, y + kBarDy, w, kBarH, kBarBack);
    if (fill > 0) rect(x0, y + kBarDy, fill, kBarH, kBarFill);
    rect(x0 + (fill > 0 ? fill - 1 : 0), y + kBarDy - 1, 2, kBarH + 2, kWhite);
    char range[12];
    range_text(e, range);
    text(kRangeX, y + kLine2Dy, range, kGray);
}

void draw_channels(std::uint32_t bits, int x, int y, int edited) {
    static constexpr char kNames[4] = {'R', 'G', 'B', 'A'};
    for (int c = 0; c < 4; ++c) {
        char t[8];
        std::snprintf(t, sizeof t, "%c%3d", kNames[c], channel(bits, c));
        text(x + c * 5 * kCell, y, t, c == edited ? kYellow : kWhite);
    }
}

void draw_tunable(const Registry& r, const Row& row, int y, bool selected) {
    const Entry& e = r.entries[row.ref];
    const bool pending = (e.flags & kPending) != 0;
    const std::uint32_t bits = pending ? e.pending : read_bits(e);
    if (pending) text(2, y + 2, "~", kCyan);
    else if (e.kind != Kind::Action && bits != e.def) text(2, y + 2, "*", kYellow);
    const int depth = row.depth;
    if (e.kind == Kind::Action) {
        rect(kMinusX, y + 1, kPlusX + kPlusW - kMinusX, 10, kButton);
        text(kMinusX + 13, y + 2, "run", kWhite);
        return;
    }
    char v[48];
    value_text(e, bits, v, sizeof v);
    text(kValueRight - width(v), y + 2, v, pending ? kCyan : kWhite);
    rect(kMinusX, y + 1, kMinusW, 10, kButton);
    text(kMinusX + 8, y + 2, "-", kWhite);
    rect(kPlusX, y + 1, kPlusW, 10, kButton);
    text(kPlusX + 8, y + 2, "+", kWhite);
    if (e.kind == Kind::Float || e.kind == Kind::Int32) {
        draw_bar(e, bits, y, depth);
    } else if (e.kind == Kind::Color) {
        const Color swatch{channel(bits, 0), channel(bits, 1), channel(bits, 2), 255};
        rect(bar_x0(depth), y + kLine2Dy, 12, 8, swatch);
        if (selected) draw_channels(bits, bar_x0(depth) + 16, y + kLine2Dy, r.panel.channel);
    } else if (e.kind == Kind::Enum) {
        char t[16];
        std::snprintf(t, sizeof t, "%u/%u", static_cast<unsigned>(bits + 1), static_cast<unsigned>(e.enum_count));
        text(bar_x0(depth), y + kLine2Dy, t, kGray);
    }
}

void draw_label(const Row& row, int y) {
    int n = 0;
    const char* label = row_label(row, n);
    char t[48];
    if (row.type == RowType::Group && row.open) std::snprintf(t, sizeof t, "- %.*s", n, label);
    else if (row.type == RowType::Group) std::snprintf(t, sizeof t, "+ %.*s (%u)", n, label, static_cast<unsigned>(row.count));
    else std::snprintf(t, sizeof t, "%.*s", n, label);
    text(kNameX + row.depth * kCell, y, t, row.type == RowType::Group ? kCyan : kWhite);
}

void watch_value(const Registry& r, const Row& row, char* out, int size) {
    const WatchValue v = row.type == RowType::Shown ? read_shown(r.shown[row.ref]) : read_watch(r.watches[row.ref]);
    std::uint32_t word = 0;
    std::memcpy(&word, v.bytes, 4);
    if (v.kind == Kind::Vec2) {
        float xy[2];
        std::memcpy(xy, v.bytes, 8);
        char t[kVec2TextBytes];
        format_vec2(xy[0], xy[1], t);
        std::snprintf(out, static_cast<std::size_t>(size), "%s", t);
    } else if (v.kind == Kind::Text) {
        format_text(reinterpret_cast<const char*>(v.bytes), v.length, out, size);
    } else {
        char t[kValueTextBytes];
        format_bits(v.kind, word, true, t);
        std::snprintf(out, static_cast<std::size_t>(size), "%s", t);
    }
}

void header(const Registry& r, int page, std::uint32_t frame, bool paused) {
    char t[64];
    if (page == kPageTunables) std::snprintf(t, sizeof t, "Tunables %d/%d", r.tunable_count, r.max_tunables);
    else std::snprintf(t, sizeof t, "Watches %d/%d", r.watch_count + r.shown_count, r.max_watches + r.max_shown);
    text(4, kHeaderY, t, kWhite);
    std::snprintf(t, sizeof t, "F %lu %s", static_cast<unsigned long>(frame), paused ? "PAUSED" : "RUN");
    text(kScreenW - 4 - width(t), kHeaderY, t, paused ? kYellow : kGray);
}

} // namespace

void panel_draw_page(const Registry& r, int page, std::uint32_t frame, bool paused) {
    header(r, page, frame, paused);
    const bool tunables = page == kPageTunables;
    const int height = tunables ? kRowHeight : kWatchRowHeight;
    const int visible = tunables ? kTunableRowsVisible : kWatchRowsVisible;
    const int scroll = panel_scroll(r, page), selected = panel_selected(r, page);
    if (row_count(r, page) == 0) text(kNameX, kRowsTop + 2, tunables ? "no tunables" : "no watches", kGray);
    for (int i = 0; i < visible; ++i) {
        Row row;
        if (!row_at(r, page, scroll + i, row)) break;
        const int y = row_y(i, height);
        if (scroll + i == selected) rect(0, y, kScreenW, height - 1, kSelected);
        draw_label(row, y + (tunables ? 2 : 1));
        if (row.type == RowType::Group) continue;
        if (tunables) {
            draw_tunable(r, row, y, scroll + i == selected);
        } else {
            char v[kQuotedTextBytes];
            watch_value(r, row, v, sizeof v);
            text(kWatchValueRight - width(v), y + 1, v, kWhite);
        }
    }
    text(4, kFooterY,
         tunables ? "A open X reset Y dump SEL+R page" : "A open B close SEL+R page SEL+L hide", kGray);
}

} // namespace nnh::detail::live
