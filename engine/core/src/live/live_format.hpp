#pragma once
// The text of live values (spec 030 plan, "Float text"): one formatter for the panel, `nnh` and the dump, so a value
// reads the same everywhere and reads back to the same bits (FR-13). Floats: Ryu's shortest round-trip digits laid out
// as plain decimals for exponents −5..8 (`5.25`, `0.00001`, `-0`, `120000000`) and `d.ddde±XX` otherwise (`1e-06`,
// `1e+09`); `inf`, `-inf`, `nan` (watches only). Integer-only code: the same text on every system. Pure, shared with
// tools/nnh. Tests: tests/live/test_live_format.cpp.

#include <cstdint>

#include "live/live_kinds.hpp"

namespace nnh::detail::live {

/// Enough for any scalar value's text and its NUL ("-1.17549435e-38", "-2147483648", "#RRGGBBAA").
inline constexpr int kValueTextBytes = 24;
/// Enough for a Vec2's text and its NUL.
inline constexpr int kVec2TextBytes = 2 * kValueTextBytes;
/// Enough for any quoted text of kMaxTextBytes bytes (every byte escaped as \xHH) and its NUL.
inline constexpr int kQuotedTextBytes = 2 + 4 * kMaxTextBytes + 2;

/// Each writes the text and a NUL into `out` and returns its length (without the NUL). Never allocate.
int format_float(float v, char out[kValueTextBytes]);
int format_int(std::int64_t v, char out[kValueTextBytes]);
int format_bool(bool v, char out[kValueTextBytes]);
/// `#RRGGBBAA`; with `short_when_opaque`, `#RRGGBB` when alpha is 255 (the panel).
int format_color(std::uint32_t rgba, bool short_when_opaque, char out[kValueTextBytes]);
/// `(x, y)`.
int format_vec2(float x, float y, char out[kVec2TextBytes]);
/// `"text"` with `"` and `\` escaped and bytes outside 0x20..0x7E as `\xHH`. Cut (still closed by `"`) when `cap` is too
/// small; `cap` >= 3.
int format_text(const char* text, int length, char* out, int cap);

/// The text of a 32-bit value of `kind` (Float, Int32, Bool, Color, Int8..UInt32, Enum as its index): what the panel
/// (`panel` = true: short colours) and the dump print. Vec2 and Text have their own functions.
int format_bits(Kind kind, std::uint32_t bits, bool panel, char out[kValueTextBytes]);

} // namespace nnh::detail::live
