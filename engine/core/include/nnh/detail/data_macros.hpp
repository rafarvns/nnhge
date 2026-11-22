#pragma once
/// @file nnh/detail/data_macros.hpp
/// NNH_DATA_FIELD / NNH_DATA_TYPE / NNH_DATA_STRUCT / NNH_DATA_ENUM (spec 014, D29). Each declaration expands to
/// one struct in an anonymous namespace (named from __COUNTER__, so qualified type names work) holding the
/// constant tables, plus a static node linked into the registry before main(). Included by
/// <nnh/detail/data_schema.hpp>; never include it on its own.

#include <cstddef> // offsetof

#define NNH_DETAIL_DATA_CAT2(a, b) a##b
#define NNH_DETAIL_DATA_CAT(a, b) NNH_DETAIL_DATA_CAT2(a, b)

/// One field of the type named by the enclosing NNH_DATA_TYPE / NNH_DATA_STRUCT, with optional rules written
/// as designated initializers of data::FieldOptions, in its order: `NNH_DATA_FIELD(hp, .min = 1, .required =
/// true)`.
#define NNH_DATA_FIELD(member, ...)                                                                          \
    ::nnh::detail::make_field<Self, decltype(Self::member)>(#member, offsetof(Self, member),                 \
                                                            ::nnh::data::FieldOptions{__VA_ARGS__})

/// Declares data type `Type` whose assets are the files `<name>.<key>` (key: 1-16 of a-z 0-9 _, starting with
/// a letter), with the fields listed (at least one). Use once per type, at namespace scope, in a schema
/// source.
#define NNH_DATA_TYPE(Type, key, ...)                                                                        \
    NNH_DETAIL_DATA_TYPE(Type, key, NNH_DETAIL_DATA_CAT(nnh_data_type_, __COUNTER__), __VA_ARGS__)

/// Declares a struct that only appears inside data types (a `[section]` or `[[blocks]]`): like NNH_DATA_TYPE
/// without a key.
#define NNH_DATA_STRUCT(Type, ...)                                                                           \
    NNH_DETAIL_DATA_TYPE(Type, "", NNH_DETAIL_DATA_CAT(nnh_data_type_, __COUNTER__), __VA_ARGS__)

/// Declares the names of a game enum, written by name in data files: `NNH_DATA_ENUM(Element, Normal, Fire,
/// Ice)`. List every enumerator a data file may use (up to 255); a name that isn't an enumerator doesn't
/// compile.
#define NNH_DATA_ENUM(Enum, ...)                                                                             \
    NNH_DETAIL_DATA_ENUM(Enum, NNH_DETAIL_DATA_CAT(nnh_data_enum_, __COUNTER__), __VA_ARGS__)

#define NNH_DETAIL_DATA_TYPE(Type, key, id, ...)                                                             \
    namespace {                                                                                              \
    struct id {                                                                                              \
        using Self = Type;                                                                                   \
        static constexpr ::nnh::detail::FieldInfo fields[] = {__VA_ARGS__};                                  \
        static constexpr ::nnh::detail::TypeInfo info =                                                      \
            ::nnh::detail::make_type<Type>(#Type, key, __FILE__, fields);                                    \
    };                                                                                                       \
    ::nnh::detail::TypeNode NNH_DETAIL_DATA_CAT(id, _node){&id::info, nullptr};                              \
    const ::nnh::detail::TypeRegistrar NNH_DETAIL_DATA_CAT(id, _registrar){NNH_DETAIL_DATA_CAT(id, _node)};  \
    }                                                                                                        \
    static_assert(true, "")

#define NNH_DETAIL_DATA_ENUM(Enum, id, ...)                                                                  \
    namespace {                                                                                              \
    struct id {                                                                                              \
        static constexpr auto values = [] {                                                                  \
            using enum Enum;                                                                                 \
            return ::nnh::detail::data_enum_values<Enum>({__VA_ARGS__});                                     \
        }();                                                                                                 \
        static constexpr ::nnh::detail::EnumInfo info =                                                      \
            ::nnh::detail::make_enum<Enum>(#Enum, #__VA_ARGS__, values, __FILE__);                           \
    };                                                                                                       \
    ::nnh::detail::EnumNode NNH_DETAIL_DATA_CAT(id, _node){&id::info, nullptr};                              \
    const ::nnh::detail::EnumRegistrar NNH_DETAIL_DATA_CAT(id, _registrar){NNH_DETAIL_DATA_CAT(id, _node)};  \
    }                                                                                                        \
    static_assert(true, "")
