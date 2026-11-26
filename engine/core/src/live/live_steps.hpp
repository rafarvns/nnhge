#pragma once
// Ranges, stepping, drag and bar fill of live values (spec 030 plan, "Ranges and stepping"). The grid of a numeric
// tunable is g(k) = min + k·step (float: min + float(k) * step; int: in 64 bits) for k = 0..n, n the largest k with
// g(k) <= max (<= 2^24); max is also a stop. Up by d: the smallest g(k) > v, then d − 1 more; past n → max. Down mirrors
// it, floored at min. Pure and built without fused multiply-add (-ffp-contract=off), shared with tools/nnh, so the panel,
// `nnh` and every system step to the same bits. Tests: tests/live/test_live_steps.cpp.

#include <cstdint>

#include "live/live_kinds.hpp"

namespace nnh::detail::live {

inline constexpr std::uint32_t kMaxGridSteps = 1u << 24;

struct FloatGrid {
    float min = 0, max = 0, step = 0;
    std::uint32_t n = 0; ///< the last grid index
};
struct IntGrid {
    std::int32_t min = 0, max = 0, step = 1;
    std::uint32_t n = 0;
};

enum class RangeError : std::uint8_t { None, NotFinite, Empty, BadStep, TooManySteps };

/// Checks a float range (finite, min < max, step > 0 and finite, n <= 2^24) and fills `out`.
RangeError make_float_grid(float min, float max, float step, FloatGrid& out);
/// Checks an int range (min < max, step > 0, n <= 2^24) and fills `out`.
RangeError make_int_grid(std::int32_t min, std::int32_t max, std::int32_t step, IntGrid& out);

/// g(k) for k <= n (g(0) is min, bit for bit).
float grid_value(const FloatGrid& g, std::uint32_t k);
std::int32_t grid_value(const IntGrid& g, std::uint32_t k);

/// `v` moved by `delta` grid steps (±1, ±10; never 0), stopping at min and max.
float step_value(const FloatGrid& g, float v, int delta);
std::int32_t step_value(const IntGrid& g, std::int32_t v, int delta);

/// The grid index under x on a bar [x0, x0 + w] (x clamped): k = ((x − x0) · n + w / 2) / w.
std::uint32_t drag_index(int x, int x0, int w, std::uint32_t n);
/// The value under x: g(k), or max when the bar's end is past g(n) (k == n and g(n) < max never: k <= n).
float drag_value(const FloatGrid& g, int x, int x0, int w);
std::int32_t drag_value(const IntGrid& g, int x, int x0, int w);

/// Pixels of a bar `w` wide filled for `v`: int((v − min) / (max − min) · w), clamped to [0, w].
int bar_fill(const FloatGrid& g, float v, int w);
int bar_fill(const IntGrid& g, std::int32_t v, int w);

/// A colour channel moved by `delta`, clamped to 0..255.
std::uint8_t step_channel(std::uint8_t v, int delta);
/// An enum index moved by `delta`, stopping at 0 and count − 1.
std::uint32_t step_enum(std::uint32_t index, std::uint32_t count, int delta);

/// Why a value is refused (the reasons of LiveRefused records).
enum class Refusal : std::uint8_t {
    None = 0,
    NotDeclared = 1,
    WrongKind = 2,
    OutOfRange = 3,
    NotFinite = 4,
    QueueFull = 5,
    FollowsFull = 6,
    NoLiveValues = 7,
};

/// What a value is checked against: the entry's kind, its min/max bits (float or int32) and its enum count.
struct ValueRule {
    Kind kind = Kind::None;
    std::uint32_t min = 0, max = 0;
    std::uint32_t count = 0; ///< enums
};

/// Refusal::None, NotFinite (a float), OutOfRange (outside [min, max], an enum index >= count, a bool other than 0/1)
/// or WrongKind (an action or a watch kind: nothing to set).
Refusal check_value(const ValueRule& rule, std::uint32_t bits);

/// A short English reason ("out of range").
const char* refusal_text(Refusal r);

} // namespace nnh::detail::live
