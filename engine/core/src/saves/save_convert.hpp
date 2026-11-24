#pragma once
// Value conversions of structured saves (spec 027, D46, FR-10, FR-11): exact only, on the bits (no FPU), the
// same on every system. Integers convert between widths and signedness when the value fits, integers to
// float/double when exactly representable, float widens to double (subnormals included), double narrows to
// float only when exact; a non-finite float, a bool byte other than 0/1, an enum value outside its
// NNH_DATA_ENUM names, or another kind doesn't fit. Tested in tests/saves/test_evolution.cpp and
// test_fit.cpp.

#include "nnh/saves.hpp"

#include <cstdint>

namespace nnh::detail {

/// Converts one value of file kind `from` at `src` into field `f`'s kind at `dst` (an enum field checks
/// `names` when not null). Returns false, `dst` unchanged, when it doesn't fit. For every kind but String and
/// Struct.
bool convert_scalar(save::WireKind from, const std::uint8_t* src, const SaveFieldInfo& f,
                    const EnumInfo* names, std::uint8_t* dst);

/// Copies the string at `src` (its length byte, then the bytes) into a data::String of `capacity` at `dst`,
/// the rest cleared. A longer string is cut at a UTF-8 character boundary: returns true when it was cut.
bool convert_string(const std::uint8_t* src, int capacity, std::uint8_t* dst);

constexpr bool f32_finite(std::uint32_t bits) {
    return (bits & 0x7F800000u) != 0x7F800000u;
}
constexpr bool f64_finite(std::uint64_t bits) {
    return (bits & 0x7FF0000000000000ull) != 0x7FF0000000000000ull;
}

/// Whether file kind `from` copies into field `f` byte for byte (same kind; no check needed per value):
/// integers, Color and references. Floats, Vec2 and bools need a check per value; enums with names too.
constexpr bool copies_as_is(save::WireKind from, const SaveFieldInfo& f) {
    if (from != f.kind) return false;
    switch (from) {
    case save::WireKind::Bool:
    case save::WireKind::F32:
    case save::WireKind::F64:
    case save::WireKind::Vec2:
    case save::WireKind::String:
    case save::WireKind::Struct:
        return false;
    default:
        return f.enum_names == nullptr;
    }
}

} // namespace nnh::detail
