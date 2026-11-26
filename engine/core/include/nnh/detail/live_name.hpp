#pragma once
/// @file nnh/detail/live_name.hpp
/// Live values' limits and name rules (spec 030), shared by <nnh/live.hpp>'s compile-time check, the registry and the
/// `nnh` tool. Engine-internal; include <nnh/live.hpp>.

#include <cstddef>
#include <cstdint>

#include "nnh/build.hpp"

namespace nnh::live {

/// Live values are compiled in (Debug and Development).
inline constexpr bool kEnabled = build::kDevToolsEnabled;
/// The longest name: ASCII letters, digits, '_', '-' and '/', with at most 3 group levels.
inline constexpr int kMaxNameBytes = 31;
/// Group levels in a name (`a/b/c/leaf`).
inline constexpr int kMaxGroupLevels = 3;
/// The most names an enum tunable may have (values 0..N-1).
inline constexpr int kMaxEnumNames = 32;

} // namespace nnh::live

namespace nnh::detail::live_name {

/// What is wrong with a name (the order of the checks: length, then characters, then parts, then depth).
enum class Error : std::uint8_t { None, TooLong, BadCharacter, EmptyPart, TooDeep };

constexpr bool name_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' ||
           c == '/';
}

/// The name rules (spec 030 FR-1, plan "Names"): 1..31 bytes of [A-Za-z0-9_-/], no empty part (leading, trailing or
/// double '/', or an empty name), at most 3 '/'. Shared by the compile-time check, the registry and `nnh`.
constexpr Error check(const char* text, std::size_t length) {
    if (length > static_cast<std::size_t>(nnh::live::kMaxNameBytes)) return Error::TooLong;
    for (std::size_t i = 0; i < length; ++i) {
        if (!name_char(text[i])) return Error::BadCharacter;
    }
    if (length == 0 || text[0] == '/' || text[length - 1] == '/') return Error::EmptyPart;
    int slashes = 0;
    for (std::size_t i = 0; i < length; ++i) {
        if (text[i] != '/') continue;
        if (text[i + 1] == '/') return Error::EmptyPart;
        ++slashes;
    }
    return slashes > nnh::live::kMaxGroupLevels ? Error::TooDeep : Error::None;
}

// Called only when a literal name breaks a rule: not constexpr, so the compiler stops and names the rule.
void live_name_too_long();
void live_name_bad_character();
void live_name_empty_part();
void live_name_too_deep();

} // namespace nnh::detail::live_name
