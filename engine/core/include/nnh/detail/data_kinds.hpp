#pragma once
/// @file nnh/detail/data_kinds.hpp
/// Compile-time deduction of a data field's kind, width, shape and range from its C++ member type (spec 014
/// FR-2). Included by <nnh/detail/data_schema.hpp>; never include it on its own.

#include <cfloat>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

#include "nnh/detail/type_id.hpp"

namespace nnh::detail {

static_assert(sizeof(bool) == 1, "data assets need a 1-byte bool");
static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559,
              "data assets need IEEE 754 floats");

template <class> inline constexpr bool data_always_false = false;

template <class E> struct DataIsList : std::false_type {};
template <class E, int N> struct DataIsList<data::List<E, N>> : std::true_type {};

template <class E> struct DataRefTarget {
    using type = void;
};
template <class T> struct DataRefTarget<data::Ref<T>> {
    using type = T;
};

template <class E> struct DataStringCapacity : std::integral_constant<int, 0> {};
template <int N> struct DataStringCapacity<data::String<N>> : std::integral_constant<int, N> {};

/// One value (One), exactly N (T[N]) or 0..N (List<T, N>) of Element.
template <class M> struct DataShape {
    using Element = M;
    static constexpr FieldShape shape = FieldShape::One;
    static constexpr int count = 1;
    static constexpr std::size_t items_offset = 0;
};
template <class E, std::size_t N> struct DataShape<E[N]> {
    static_assert(N <= static_cast<std::size_t>(data::kMaxListItems),
                  "an array in a data type holds at most 256 values");
    using Element = E;
    static constexpr FieldShape shape = FieldShape::Array;
    static constexpr int count = static_cast<int>(N);
    static constexpr std::size_t items_offset = 0;
};
template <class E, int N> struct DataShape<data::List<E, N>> {
    using Element = E;
    static constexpr FieldShape shape = FieldShape::List;
    static constexpr int count = N;
    static constexpr std::size_t items_offset = data::List<E, N>::items_offset();
};

/// Stops the build, with a message saying what to use instead, for member types data files can't hold.
template <class E> constexpr void data_check_element() {
    if constexpr (std::is_pointer_v<E> || std::is_reference_v<E> || std::is_member_pointer_v<E>) {
        static_assert(data_always_false<E>,
                      "data fields can't be pointers or references: use data::Ref<T>, data::TextureRef, "
                      "data::SoundRef or data::MusicRef");
    } else if constexpr (std::is_array_v<E> || DataIsList<E>::value) {
        static_assert(data_always_false<E>, "arrays and lists of arrays or lists aren't data fields");
    } else if constexpr (std::is_floating_point_v<E> && !std::is_same_v<E, float>) {
        static_assert(data_always_false<E>, "double isn't a data field kind: use float");
    } else if constexpr (std::is_same_v<E, char> || std::is_same_v<E, wchar_t> ||
                         std::is_same_v<E, char8_t> || std::is_same_v<E, char16_t> ||
                         std::is_same_v<E, char32_t>) {
        static_assert(
            data_always_false<E>,
            "character types aren't data field kinds: use std::int8_t, std::uint8_t or data::String<N>");
    } else if constexpr (std::is_integral_v<E> && sizeof(E) > 4) {
        static_assert(data_always_false<E>, "64-bit integers aren't data field kinds: use 8, 16 or 32 bits");
    } else if constexpr (std::is_union_v<E>) {
        static_assert(data_always_false<E>, "unions aren't data field kinds");
    } else if constexpr (!std::is_trivially_copyable_v<E>) {
        static_assert(
            data_always_false<E>,
            "data fields must be trivially copyable: use data::String<N> and data::List<T, N> instead of "
            "std::string and std::vector");
    } else if constexpr (!std::is_arithmetic_v<E> && !std::is_enum_v<E> && !std::is_class_v<E>) {
        static_assert(data_always_false<E>, "unsupported data field type");
    }
}

template <class E> constexpr FieldKind data_kind_of() {
    if constexpr (std::is_same_v<E, bool>)
        return FieldKind::Bool;
    else if constexpr (std::is_same_v<E, float>)
        return FieldKind::Float;
    else if constexpr (std::is_enum_v<E>)
        return FieldKind::Enum;
    else if constexpr (std::is_integral_v<E>)
        return FieldKind::Int;
    else if constexpr (DataStringCapacity<E>::value > 0)
        return FieldKind::String;
    else if constexpr (std::is_same_v<E, Vec2>)
        return FieldKind::Vec2;
    else if constexpr (std::is_same_v<E, Color>)
        return FieldKind::Color;
    else if constexpr (!std::is_void_v<typename DataRefTarget<E>::type>)
        return FieldKind::DataRef;
    else if constexpr (std::is_same_v<E, data::TextureRef>)
        return FieldKind::TextureRef;
    else if constexpr (std::is_same_v<E, data::SoundRef>)
        return FieldKind::SoundRef;
    else if constexpr (std::is_same_v<E, data::MusicRef>)
        return FieldKind::MusicRef;
    else
        return FieldKind::Struct;
}

template <class E> constexpr std::uint8_t data_width_of() {
    constexpr FieldKind k = data_kind_of<E>();
    if constexpr (k == FieldKind::Bool || k == FieldKind::Float || k == FieldKind::Int ||
                  k == FieldKind::Enum) {
        return static_cast<std::uint8_t>(sizeof(E));
    } else {
        return 0;
    }
}

template <class E> constexpr bool data_signed_of() {
    if constexpr (std::is_enum_v<E>)
        return std::is_signed_v<std::underlying_type_t<E>>;
    else if constexpr (std::is_integral_v<E> && !std::is_same_v<E, bool>)
        return std::is_signed_v<E>;
    else
        return false;
}

template <class E> constexpr const void* data_target_of() {
    constexpr FieldKind k = data_kind_of<E>();
    if constexpr (k == FieldKind::Enum || k == FieldKind::Struct)
        return type_id<E>();
    else if constexpr (k == FieldKind::DataRef)
        return type_id<typename DataRefTarget<E>::type>();
    else
        return nullptr;
}

/// The values a member of type E can hold (numbers only; 0..0 otherwise).
template <class E> struct DataRange {
    static constexpr double lowest = 0;
    static constexpr double highest = 0;
};
template <class E>
    requires(std::is_integral_v<E> && !std::is_same_v<E, bool>)
struct DataRange<E> {
    static constexpr double lowest = static_cast<double>(std::numeric_limits<E>::lowest());
    static constexpr double highest = static_cast<double>(std::numeric_limits<E>::max());
};
template <> struct DataRange<float> {
    static constexpr double lowest = -static_cast<double>(FLT_MAX);
    static constexpr double highest = static_cast<double>(FLT_MAX);
};

constexpr bool data_whole(double v) {
    if (v <= -9.0e18 || v >= 9.0e18) return true; // beyond any 32-bit range: only clamps
    return v == static_cast<double>(static_cast<long long>(v));
}

constexpr bool data_same_text(const char* a, const char* b) {
    while (*a != '\0' && *a == *b) {
        ++a;
        ++b;
    }
    return *a == *b;
}

/// Names in NNH_DATA_ENUM's stringified list ("A, B, C" → 3).
constexpr int data_count_names(const char* names) {
    if (*names == '\0') return 0;
    int n = 1;
    for (; *names != '\0'; ++names) n += *names == ',' ? 1 : 0;
    return n;
}

} // namespace nnh::detail
