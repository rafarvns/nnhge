#pragma once
/// @file nnh/detail/data_tables.hpp
/// Builders of the schema records behind the NNH_DATA_* macros (spec 014, D29): make_field (a member's kind,
/// shape and range deduced at compile time), make_type and make_enum. Included by <nnh/detail/data_schema.hpp>;
/// never include it on its own.

#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>

#include "nnh/detail/data_kinds.hpp"

namespace nnh::detail {

/// The record of field `name` (a member of type M at `offset` in Self). See NNH_DATA_FIELD.
template <class Self, class M>
constexpr FieldInfo make_field(const char* name, std::size_t offset, const data::FieldOptions& options) {
    static_assert(!std::is_const_v<M>, "data fields can't be const: the loader writes them");
    using Shape = DataShape<std::remove_volatile_t<M>>;
    using E = typename Shape::Element;
    data_check_element<E>();
    FieldInfo f;
    f.name = name;
    f.offset = static_cast<std::uint32_t>(offset);
    f.stride = static_cast<std::uint32_t>(sizeof(E));
    f.items_offset = static_cast<std::uint32_t>(Shape::items_offset);
    f.count = static_cast<std::uint16_t>(Shape::count);
    f.shape = Shape::shape;
    f.kind = data_kind_of<E>();
    f.width = data_width_of<E>();
    f.is_signed = data_signed_of<E>();
    f.capacity = static_cast<std::uint16_t>(DataStringCapacity<E>::value);
    f.target = data_target_of<E>();
    f.required = options.required;
    const bool numeric = f.kind == FieldKind::Int || f.kind == FieldKind::Float;
    const bool bounded = options.min != data::FieldOptions{}.min || options.max != data::FieldOptions{}.max;
    if (!numeric) {
        if (bounded) data_error_min_or_max_on_a_field_that_is_not_a_number();
        return f;
    }
    if (f.kind == FieldKind::Int && bounded && !(data_whole(options.min) && data_whole(options.max))) {
        data_error_min_or_max_of_an_integer_field_must_be_a_whole_number();
    }
    const double lowest = DataRange<E>::lowest;
    const double highest = DataRange<E>::highest;
    f.min = options.min > lowest ? options.min : lowest;
    f.max = options.max < highest ? options.max : highest;
    if (f.min > f.max) data_error_min_is_greater_than_max();
    return f;
}

/// The record of data type T with the fields `fields`. See NNH_DATA_TYPE.
template <class T, std::size_t N>
constexpr TypeInfo make_type(const char* name, const char* key, const char* source,
                             const FieldInfo (&fields)[N]) {
    static_assert(std::is_class_v<T> && std::is_aggregate_v<T>,
                  "a data type must be a plain struct (an aggregate: no constructors of its own, no virtual "
                  "functions)");
    static_assert(std::is_standard_layout_v<T>,
                  "a data type must be standard-layout (all members public, no virtual)");
    static_assert(std::is_trivially_copyable_v<T>, "a data type must be trivially copyable");
    static_assert(sizeof(T) <= data::kMaxStructBytes,
                  "a data type can be at most 16 KiB (data::kMaxStructBytes)");
    static_assert(N <= static_cast<std::size_t>(data::kMaxFields), "a data type can list at most 64 fields");
    for (std::size_t i = 0; i < N; ++i) {
        for (std::size_t j = 0; j < i; ++j) {
            if (data_same_text(fields[i].name, fields[j].name)) data_error_field_listed_twice();
        }
    }
    TypeInfo t;
    t.tag = type_id<T>();
    t.name = name;
    t.key = key;
    t.source = source;
    t.fields = fields;
    t.field_count = static_cast<std::uint16_t>(N);
    t.size = static_cast<std::uint32_t>(sizeof(T));
    t.reset = [](void* p) { ::new (p) T{}; };
    return t;
}

/// Values of an enum's names, in order (NNH_DATA_ENUM builds it from a braced list of the enumerators).
template <std::size_t N> struct DataEnumValues {
    std::int64_t v[N];
};

template <class E, std::size_t N> constexpr DataEnumValues<N> data_enum_values(const E (&list)[N]) {
    static_assert(std::is_enum_v<E>, "NNH_DATA_ENUM needs an enum or enum class");
    static_assert(N <= 255, "NNH_DATA_ENUM: at most 255 names");
    static_assert(sizeof(E) <= 4, "NNH_DATA_ENUM: the enum's underlying type can be at most 32 bits");
    DataEnumValues<N> out{};
    for (std::size_t i = 0; i < N; ++i) out.v[i] = static_cast<std::int64_t>(list[i]);
    return out;
}

template <class E, std::size_t N>
constexpr EnumInfo make_enum(const char* name, const char* names, const DataEnumValues<N>& values,
                             const char* source) {
    if (data_count_names(names) != static_cast<int>(N)) data_error_enum_names_and_values_differ();
    EnumInfo e;
    e.tag = type_id<E>();
    e.name = name;
    e.names = names;
    e.values = values.v;
    e.source = source;
    e.count = static_cast<std::uint16_t>(N);
    e.width = static_cast<std::uint8_t>(sizeof(E));
    e.is_signed = std::is_signed_v<std::underlying_type_t<E>>;
    return e;
}

} // namespace nnh::detail
