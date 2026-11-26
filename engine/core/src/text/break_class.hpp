#pragma once
// Line-break classes of code points (spec 029 FR-10, FR-12; plan 029 "Wrapping"): break spaces (U+0020, U+3000),
// hyphens (U+002D, U+2010), ideographic characters (ID: CJK ideographs, kana, fullwidth forms — a line may break
// between two of them; Hangul is not ID), the kinsoku tables (JIS X 4051 classes: characters no line starts with, or
// ends with), and characters that draw nothing and advance nothing (combining marks, variation selectors, ZWSP/ZWNJ/ZWJ).
// Below U+0100 only the space and '-' have a class, so Latin-1 text wraps exactly as spec 028's wrapper did. Pure, no
// engine includes: nnh-assetc stores the same bits in glyph records. Tests: tests/text/test_break_class.cpp.

#include <cstdint>

namespace nnh::detail {

inline constexpr std::uint8_t kBreakKindMask = 3;
inline constexpr std::uint8_t kBreakOther = 0;
inline constexpr std::uint8_t kBreakSpace = 1;       ///< a break opportunity; dropped at a break
inline constexpr std::uint8_t kBreakHyphen = 2;      ///< a break opportunity after it
inline constexpr std::uint8_t kBreakIdeographic = 3; ///< a break opportunity between two of them
inline constexpr std::uint8_t kBreakNoStart = 4;     ///< kinsoku: no line starts with it
inline constexpr std::uint8_t kBreakNoEnd = 8;       ///< kinsoku: no line ends with it
inline constexpr std::uint8_t kBreakZeroWidth = 16;  ///< draws nothing, advances nothing (game fonts)
inline constexpr std::uint8_t kBreakBits = 31;

/// The break bits of `c` (a kind and flags). Constant time below U+0300, a binary search of 129 ranges above.
std::uint8_t break_class(char32_t c);

} // namespace nnh::detail
