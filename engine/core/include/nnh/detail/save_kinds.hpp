#pragma once
/// @file nnh/detail/save_kinds.hpp
/// Compile-time deduction of a saved field's wire kind, shape and sizes from its C++ member type (spec 027
/// FR-3), over spec 014's value types and traits (data::String, data::List, data::Ref, DataStringCapacity,
/// DataIsList, DataRefTarget; D29 read, not changed). Adds what spec 014 refuses for data files: double,
/// 64-bit integers and arrays of up to 65535 items, with its own element checks. Included by <nnh/saves.hpp>;
/// never on its own.

#include "nnh/detail/type_id.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace nnh::detail {

static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559,
              "structured saves need IEEE 754 doubles");
static_assert(sizeof(Vec2) == 8, "structured saves store Vec2 as two floats");

template <class> inline constexpr bool save_always_false = false;

/// One value (One), exactly N (T[N]) or 0..N (data::List<T, N>) of Element.
template <class M> struct SaveShape {
    using Element = M;
    static constexpr save::WireShape shape = save::WireShape::One;
    static constexpr int count = 1;
    static constexpr std::size_t items_offset = 0;
};
template <class E, std::size_t N> struct SaveShape<E[N]> {
    static_assert(N <= static_cast<std::size_t>(save::kMaxArrayItems),
                  "an array in a save holds at most 65535 items");
    using Element = E;
    static constexpr save::WireShape shape = save::WireShape::Array;
    static constexpr int count = static_cast<int>(N);
    static constexpr std::size_t items_offset = 0;
};
template <class E, int N> struct SaveShape<data::List<E, N>> {
    using Element = E;
    static constexpr save::WireShape shape = save::WireShape::List;
    static constexpr int count = N;
    static constexpr std::size_t items_offset = data::List<E, N>::items_offset();
};

/// A struct member with its own save declaration (found by argument-dependent lookup in its namespace).
template <class T> inline constexpr bool save_declared = requires { nnh_save_info(SaveTag<T>{}); };

/// Stops the build, with a message saying what to use instead, for member types a save can't hold.
template <class E> constexpr void save_check_element() {
    if constexpr (std::is_pointer_v<E> || std::is_reference_v<E> || std::is_member_pointer_v<E>) {
        static_assert(save_always_false<E>, "saved fields can't be pointers or references: use data::Ref<T>, "
                                            "data::TextureRef, data::SoundRef, data::MusicRef or an id");
    } else if constexpr (std::is_array_v<E> || DataIsList<E>::value) {
        static_assert(save_always_false<E>,
                      "arrays and lists of arrays or lists aren't saved fields: wrap the inner "
                      "one in a struct with NNH_SAVE_STRUCT");
    } else if constexpr (std::is_same_v<E, char> || std::is_same_v<E, wchar_t> ||
                         std::is_same_v<E, char8_t> || std::is_same_v<E, char16_t> ||
                         std::is_same_v<E, char32_t>) {
        static_assert(save_always_false<E>, "character types aren't saved field kinds: use std::int8_t, "
                                            "std::uint8_t or data::String<N>");
    } else if constexpr (std::is_floating_point_v<E> && !std::is_same_v<E, float> &&
                         !std::is_same_v<E, double>) {
        static_assert(save_always_false<E>, "long double isn't a saved field kind: use double");
    } else if constexpr (std::is_union_v<E>) {
        static_assert(save_always_false<E>, "unions aren't saved field kinds");
    } else if constexpr (std::is_const_v<E> || std::is_volatile_v<E>) {
        static_assert(save_always_false<E>,
                      "saved fields can't be const or volatile: the loader writes them");
    } else if constexpr (!std::is_trivially_copyable_v<E>) {
        static_assert(save_always_false<E>,
                      "saved fields must be trivially copyable: use data::String<N> and "
                      "data::List<T, N> instead of std::string and std::vector");
    } else if constexpr (!std::is_arithmetic_v<E> && !std::is_enum_v<E> && !std::is_class_v<E>) {
        static_assert(save_always_false<E>, "unsupported saved field type");
    } else if constexpr (std::is_enum_v<E>) {
        using U = std::underlying_type_t<E>;
        static_assert(
            !std::is_same_v<U, char> && !std::is_same_v<U, wchar_t> && !std::is_same_v<U, bool>,
            "a saved enum's underlying type must be a <cstdint> integer (char's sign differs between "
            "systems)");
    }
}

template <class I> constexpr save::WireKind save_integer_kind() {
    constexpr bool s = std::is_signed_v<I>;
    if constexpr (sizeof(I) == 1)
        return s ? save::WireKind::I8 : save::WireKind::U8;
    else if constexpr (sizeof(I) == 2)
        return s ? save::WireKind::I16 : save::WireKind::U16;
    else if constexpr (sizeof(I) == 4)
        return s ? save::WireKind::I32 : save::WireKind::U32;
    else
        return s ? save::WireKind::I64 : save::WireKind::U64;
}

/// The wire kind of one value of member type E (an enum: its underlying integer kind).
template <class E> constexpr save::WireKind save_kind_of() {
    if constexpr (std::is_same_v<E, bool>)
        return save::WireKind::Bool;
    else if constexpr (std::is_enum_v<E>)
        return save_integer_kind<std::underlying_type_t<E>>();
    else if constexpr (std::is_integral_v<E>)
        return save_integer_kind<E>();
    else if constexpr (std::is_same_v<E, float>)
        return save::WireKind::F32;
    else if constexpr (std::is_same_v<E, double>)
        return save::WireKind::F64;
    else if constexpr (DataStringCapacity<E>::value > 0)
        return save::WireKind::String;
    else if constexpr (std::is_same_v<E, Vec2>)
        return save::WireKind::Vec2;
    else if constexpr (std::is_same_v<E, Color>)
        return save::WireKind::Color;
    else if constexpr (!std::is_void_v<typename DataRefTarget<E>::type>)
        return save::WireKind::DataRef;
    else if constexpr (std::is_same_v<E, data::TextureRef>)
        return save::WireKind::TextureRef;
    else if constexpr (std::is_same_v<E, data::SoundRef>)
        return save::WireKind::SoundRef;
    else if constexpr (std::is_same_v<E, data::MusicRef>)
        return save::WireKind::MusicRef;
    else
        return save::WireKind::Struct;
}

/// The record of a nested save struct (a build error naming the fix when it has no declaration).
template <class E> constexpr const SaveTypeInfo* save_nested_of() {
    if constexpr (save_kind_of<E>() != save::WireKind::Struct) {
        return nullptr;
    } else {
        static_assert(save_declared<E>,
                      "a struct member of a save needs its own NNH_SAVE_STRUCT, declared before "
                      "this declaration in the struct's namespace");
        if constexpr (save_declared<E>) {
            return &nnh_save_info(SaveTag<E>{});
        } else {
            return nullptr;
        }
    }
}

/// The NNH_DATA_ENUM names of enum E, looked up the first time a load needs them.
template <class E> const EnumInfo* save_enum_names_of() {
    return save_find_enum_names(type_id<E>());
}

template <class E> constexpr const EnumInfo* (*save_enum_lookup_of())() {
    if constexpr (std::is_enum_v<E>)
        return &save_enum_names_of<E>;
    else
        return nullptr;
}

} // namespace nnh::detail
