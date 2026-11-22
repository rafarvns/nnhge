#pragma once
/// @file nnh/detail/name_hash.hpp
/// The engine's one 32-bit name hash: FNV-1a 32 (decision D31 note; plan 014 path hashes, plan 016 sprite
/// names). constexpr, so a hash can be a `case` label; header-only with no includes beyond
/// <cstddef>/<cstdint>, so nnh-assetc and the schema host program compute exactly the same values as the
/// console. Engine-internal.

#include <cstddef>
#include <cstdint>

namespace nnh::detail {

/// FNV-1a 32 of the `size` bytes at `text` (bytes read as unsigned).
constexpr std::uint32_t fnv1a32(const char* text, std::size_t size) {
    std::uint32_t hash = 0x811C9DC5u;
    for (std::size_t i = 0; i < size; ++i) {
        hash ^= static_cast<std::uint8_t>(text[i]);
        hash *= 0x01000193u;
    }
    return hash;
}

/// FNV-1a 32 of a NUL-terminated string.
constexpr std::uint32_t fnv1a32(const char* text) {
    std::size_t size = 0;
    while (text[size] != '\0')
        ++size;
    return fnv1a32(text, size);
}

} // namespace nnh::detail
