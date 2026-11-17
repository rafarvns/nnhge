#pragma once
/// @file nnh/detail/type_id.hpp
/// Per-type identifiers without RTTI: the address of a variable that exists once per type.
/// Used by the event bus to tell event types apart. Engine-internal, but in a public header because
/// the templates in <nnh/events.hpp> need it.

#include <type_traits>

namespace nnh::detail {

using TypeId = const void*;

template <typename T>
struct TypeTag {
    // A static constexpr data member is implicitly inline: one object (one address) per type across the
    // whole program, even when used from several translation units and libraries.
    static constexpr char value = 0;
};

/// Unique, constant id of `T` (cv-qualifiers ignored).
template <typename T>
constexpr TypeId type_id() {
    return &TypeTag<std::remove_cv_t<T>>::value;
}

} // namespace nnh::detail
