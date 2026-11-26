#pragma once
// PC simulator: the system language's sources (spec 029 FR-27). Pure and SDL-free (tests/platform_conformance/
// test_language_pc.cpp): language.ini comes from the user's disk, so it is read bounded and every value validated.

#include <cstddef>

namespace nnh::platform::pc {

inline constexpr std::size_t kMaxLanguageIniBytes = 1024;
inline constexpr std::size_t kMaxLanguageIniLine = 128;

/// Reads `size` bytes of `text` (no terminator needed, at most kMaxLanguageIniBytes): the first `language=<code>` line
/// whose code is well-formed (spaces around key and value allowed; '#' and ';' lines, other keys and lines longer than
/// kMaxLanguageIniLine ignored) is copied into `out`. @return false (`out` unchanged) when there is none.
bool language_from_ini(const char* text, std::size_t size, char out[16]);

} // namespace nnh::platform::pc
