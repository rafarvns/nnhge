#pragma once
/// @file nnh/detail/save_macros.hpp
/// NNH_SAVE_TYPE / NNH_SAVE_STRUCT / NNH_SAVE_SUMMARY / NNH_SAVE_FIELD / NNH_SAVE_RETIRED / NNH_SAVE_VERSION
/// / NNH_SAVE_STEP (spec 027, D45). Each declaration expands, at namespace scope in the struct's own
/// namespace, to a struct `nnh_save_tables_<Type>` of constexpr tables and a constexpr function
/// `nnh_save_info(SaveTag<TypeName>)` that returns its record; the engine's templates find it by
/// argument-dependent lookup. Included by <nnh/saves.hpp>; never include it on its own.

#include <cstddef> // offsetof

// Member and type names are kept in Debug and Development (dumps, report messages); Release keeps none.
#if defined(NNH_FLAVOR_RELEASE)
#define NNH_DETAIL_SAVE_NAME(text) ""
#else
#define NNH_DETAIL_SAVE_NAME(text) text
#endif

/// A saved member and its id (1..65535, unique in its struct, never reused with another meaning):
/// `NNH_SAVE_FIELD(level, 2)`.
#define NNH_SAVE_FIELD(member, id)                                                                           \
    ::nnh::detail::save_field_entry<decltype(Self::member)>(NNH_DETAIL_SAVE_NAME(#member),                   \
                                                            offsetof(Self, member), id)

/// Ids of fields removed in earlier versions: never reusable (a build error), skipped silently when a file
/// has them.
#define NNH_SAVE_RETIRED(...) ::nnh::detail::save_retired_entry({__VA_ARGS__})

/// The save type's current version (1 when not given). A file of an older version runs the steps from its
/// version.
#define NNH_SAVE_VERSION(version) ::nnh::detail::save_version_entry(version)

/// `void function(Type& value, const nnh::save::OldFields& old)` converts a value of version `from` into
/// from + 1. Every version below the current one needs exactly one step (a build error otherwise).
#define NNH_SAVE_STEP(from, function) ::nnh::detail::save_step_entry<Self, function>(from)

/// Declares save type `Type` (a plain struct, named without qualification, at namespace scope in its own
/// namespace) whose files have the key `key` (1-16 of a-z 0-9 _, starting with a letter): its saved fields,
/// retired ids, version and migration steps, in any order.
#define NNH_SAVE_TYPE(TypeName, key, ...)                                                                    \
    NNH_DETAIL_SAVE_DECLARE(TypeName, key, ::nnh::detail::SaveRole::Type, 0, __VA_ARGS__)

/// Declares a struct that only appears inside saves (nested, or the items of an array or list): fields and
/// retired ids, no key, version or steps. Declare it before the declarations that use it.
#define NNH_SAVE_STRUCT(TypeName, ...)                                                                       \
    NNH_DETAIL_SAVE_DECLARE(TypeName, "", ::nnh::detail::SaveRole::Struct, 0, __VA_ARGS__)

/// Declares a partial view of save type `key` (a "Load game" summary): a subset of its ids, the highest save
/// version it reads. It loads with the same checks, runs no step, and can't be stored.
#define NNH_SAVE_SUMMARY(TypeName, key, version, ...)                                                        \
    NNH_DETAIL_SAVE_DECLARE(TypeName, key, ::nnh::detail::SaveRole::Summary, version, __VA_ARGS__)

#define NNH_DETAIL_SAVE_DECLARE(TypeName, key, role, version, ...)                                           \
    struct nnh_save_tables_##TypeName {                                                                      \
        using Self = TypeName;                                                                               \
        static constexpr ::nnh::detail::SaveEntry entries[] = {__VA_ARGS__};                                 \
        static constexpr ::nnh::detail::SaveCounts counts = ::nnh::detail::save_count(entries);              \
        static constexpr auto fields = ::nnh::detail::save_sorted_fields<counts.fields>(entries);            \
        static constexpr auto retired = ::nnh::detail::save_sorted_retired<counts.retired>(entries);         \
        static constexpr auto steps = ::nnh::detail::save_sorted_steps<counts.steps>(entries);               \
        static constexpr ::nnh::detail::SaveTypeInfo info = ::nnh::detail::make_save_type<TypeName>(         \
            NNH_DETAIL_SAVE_NAME(#TypeName), key, role, version, counts, fields, retired, steps);            \
    };                                                                                                       \
    constexpr const ::nnh::detail::SaveTypeInfo& nnh_save_info(::nnh::detail::SaveTag<TypeName>) {           \
        return nnh_save_tables_##TypeName::info;                                                             \
    }                                                                                                        \
    static_assert(true, "")
