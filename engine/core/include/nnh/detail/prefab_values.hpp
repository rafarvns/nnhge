#pragma once
/// @file nnh/detail/prefab_values.hpp
/// The typed edge of <nnh/prefabs.hpp> (spec 021, D36): a component value seen through its type id, size, alignment
/// and copy/destroy functions, and the placement member. Each template only fills a small struct; everything else is
/// out of line (src/prefabs/). Engine-internal: use <nnh/prefabs.hpp>.

#include <cstddef>
#include <cstdint>
#include <new>
#include <type_traits>

#include "nnh/detail/type_id.hpp"
#include "nnh/math.hpp"
#include "nnh/transform.hpp"

namespace nnh {

/// A component kind in a prefab's queries: `nnh::kind_id<Body>()` (the address-per-type id of spec 020's kinds).
using KindId = detail::TypeId;
template <typename T>
constexpr KindId kind_id() {
    return detail::type_id<T>();
}

/// One component value, type-erased (the array form of PrefabLibrary::add and spawn_with): made by
/// component_value(). Points at the game's value, which is copied by the call that takes it, never kept.
struct ComponentValue {
    KindId id = nullptr;
    const void* value = nullptr;
    int size = 0;
    int align = 0;
    void (*copy)(void* to, const void* from) = nullptr; ///< placement-new copy construction
    void (*destroy)(void*) = nullptr;                   ///< null when trivially destructible
};

/// A prefab's placement member (spec 021 FR-7): where a spawn's position goes. Made by place().
struct Placement {
    KindId kind = nullptr;
    std::uint16_t offset = 0; ///< the member's byte offset in the component
    bool rect = false;        ///< a Rect member (x, y; and w, h from a rectangle map object); else a Vec2
    bool keep_size = false;   ///< a rectangle map object doesn't change the member's w, h
};

namespace detail {

template <typename T>
void copy_component(void* to, const void* from) {
    ::new (to) T(*static_cast<const T*>(from));
}

template <typename T>
void destroy_value(void* p) {
    static_cast<T*>(p)->~T();
}

/// True when no type of the list appears twice.
template <typename T, typename... Rest>
constexpr bool distinct_types() {
    if constexpr (sizeof...(Rest) == 0) {
        return true;
    } else {
        return (!std::is_same_v<T, Rest> && ...) && distinct_types<Rest...>();
    }
}

} // namespace detail

/// The type-erased view of `value` (T: copy-constructible, alignof ≤ 16; checked here at compile time).
template <typename T>
ComponentValue component_value(const T& value) {
    static_assert(std::is_copy_constructible_v<T>,
                  "nnh::prefabs: a prefab value's type must be copy-constructible (add non-copyable components "
                  "from the spawn hook)");
    static_assert(alignof(T) <= 16, "nnh::prefabs: a prefab value's type may be aligned to at most 16 bytes");
    static_assert(std::is_nothrow_destructible_v<T>, "nnh::prefabs: T's destructor must not throw");
    ComponentValue v;
    v.id = detail::type_id<T>();
    v.value = &value;
    v.size = static_cast<int>(sizeof(T));
    v.align = static_cast<int>(alignof(T));
    v.copy = &detail::copy_component<T>;
    v.destroy = std::is_trivially_destructible_v<T> ? nullptr : &detail::destroy_value<T>;
    return v;
}

/// `nnh::place(&Body::box)`: the member that receives a spawn's position. M is nnh::Vec2, nnh::Rect or nnh::Transform2D
/// (its position: `nnh::place(&nnh::Node::local)`, spec 022) (checked at compile time); `keep_size` keeps a Rect's w, h
/// when the spawn comes from a rectangle map object. Builds one C{} to measure the member's offset (C is
/// default-constructible, as every component).
template <typename C, typename M>
Placement place(M C::*member, bool keep_size = false) {
    static_assert(std::is_same_v<M, Vec2> || std::is_same_v<M, Rect> || std::is_same_v<M, Transform2D>,
                  "nnh::place: the placement member must be an nnh::Vec2, an nnh::Rect or an nnh::Transform2D");
    static_assert(sizeof(C) <= 0xFFFF, "nnh::place: the component is too large");
    const C probe{};
    const auto* base = reinterpret_cast<const unsigned char*>(&probe);
    const auto* at = reinterpret_cast<const unsigned char*>(&(probe.*member));
    Placement p;
    p.kind = detail::type_id<C>();
    p.offset = static_cast<std::uint16_t>(at - base);
    p.rect = std::is_same_v<M, Rect>;
    p.keep_size = keep_size;
    return p;
}

} // namespace nnh
