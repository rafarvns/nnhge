#pragma once
// Text fitting and list rows for NNH Link's views: pure, shared by the views and the touch handling;
// unit tested in tests/test_layout.cpp.

#include <cstddef>
#include <string>
#include <vector>

namespace link_app::layout {

// Lists on the bottom screen (pixels).
inline constexpr int kRowTop = 8;
inline constexpr int kRowHeight = 22;
inline constexpr int kVisibleRows = 10;

/// The first row shown so that `selected` is visible.
int first_row(int selected);
/// The row under the bottom-screen point y, or -1.
int row_at(int y, int selected, int rows);

/// The button at the bottom of the bottom screen (cancel, back).
inline constexpr int kButtonX = 100, kButtonY = 196, kButtonW = 120, kButtonH = 32;
bool in_button(int x, int y);

/// `text` cut to `max` characters with "..." (never in the middle of a UTF-8 character).
std::string fit(const std::string& text, std::size_t max);
/// Each line split into rows of at most `width` characters (UTF-8 aware); tabs become spaces.
std::vector<std::string> wrap(const std::vector<std::string>& lines, std::size_t width);

} // namespace link_app::layout
