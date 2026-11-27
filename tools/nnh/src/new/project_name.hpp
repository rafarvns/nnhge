#pragma once
// The values `nnh new` writes into a project (spec 033 FR-4, FR-11, D56 item 2): the game name (its CMake
// target, folder and file names), the title and author (CMake strings and README text) and the .cia unique
// ID. Pure; tested in tools/nnh/tests/test_project_name.cpp.

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace nnh::tool {

struct NameCheck {
    bool ok = false;
    std::string rule; ///< the failed rule, e.g. "only lowercase letters, digits, '_' and '-'"; "" when ok
    std::string suggestion; ///< a valid spelling when one is obvious ("My Game" -> "my-game"), else ""
};

/// `^[a-z][a-z0-9_-]{0,31}$`, no Windows device name (con, nul, aux, prn, com1-9, lpt1-9), no target name
/// CMake or the engine uses in a game's build (dist, all, clean, install, test, help, package, ..., nnh,
/// nnh_*, nnh-*).
NameCheck check_name(std::string_view name);

struct TextCheck {
    bool ok = false;
    std::string rule; ///< "" when ok
};

/// A title or an author: valid UTF-8, at most 64 characters, no control characters and none of `" \ $ ; @ ``
/// ` `` (CMake or the README would read them). Empty only when `allow_empty` (an author not given).
TextCheck check_text_field(std::string_view text, bool allow_empty);

inline constexpr std::uint32_t kUniqueIdMin = 0x00300; ///< the homebrew range (docs/guides/packaging.md)
inline constexpr std::uint32_t kUniqueIdMax = 0xF7FFF;

/// "0x5A3C1", "0X5a3c1" or "5A3C1" within [kUniqueIdMin, kUniqueIdMax]; nullopt with `error` set otherwise.
std::optional<std::uint32_t> parse_unique_id(std::string_view text, std::string& error);

/// "0x5A3C1" (upper-case hex, at least five digits), as written in CMakeLists.txt.
std::string format_unique_id(std::uint32_t id);

} // namespace nnh::tool
