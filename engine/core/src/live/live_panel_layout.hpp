#pragma once
// The live panel's integer geometry on the bottom screen (320 × 240, the built-in font's 8 × 8 cells, line 10), shared by
// drawing and touch so a tap lands where the row is drawn (plan "Panel", layout). Pure.

namespace nnh::detail::live {

inline constexpr int kCell = 8;
inline constexpr int kHeaderY = 2;        ///< header text
inline constexpr int kRowsTop = 14;       ///< first row
inline constexpr int kRowHeight = 22;     ///< Tunables rows (2 lines)
inline constexpr int kWatchRowHeight = 11; ///< Watches rows (1 line)
inline constexpr int kFooterY = 229;      ///< footer text
inline constexpr int kNameX = 10;         ///< name at kNameX + depth · 8
inline constexpr int kValueRight = 262;   ///< values right-aligned here (Tunables)
inline constexpr int kWatchValueRight = 316;
inline constexpr int kMinusX = 266, kMinusW = 24; ///< the `-` button
inline constexpr int kPlusX = 292, kPlusW = 24;   ///< the `+` button (and an action's `run`: kMinusX..kPlusX + kPlusW)
inline constexpr int kBarRight = 178;     ///< the bar spans [8 + depth · 8, kBarRight]
inline constexpr int kBarDy = 13, kBarH = 6;
inline constexpr int kRangeX = 182;       ///< `min..max`, at most 10 characters
inline constexpr int kLine2Dy = 12;
inline constexpr int kScreenW = 320;

constexpr int row_y(int visible_index, int row_height) { return kRowsTop + visible_index * row_height; }
constexpr int bar_x0(int depth) { return 8 + depth * kCell; }
constexpr int bar_w(int depth) { return kBarRight - bar_x0(depth); }

/// The visible row index under y on a page with rows of `row_height`, or -1.
constexpr int row_under(int y, int row_height, int visible_rows) {
    if (y < kRowsTop) return -1;
    const int i = (y - kRowsTop) / row_height;
    return i < visible_rows ? i : -1;
}

} // namespace nnh::detail::live
