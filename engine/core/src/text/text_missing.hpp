#pragma once
// Missing characters logged once (spec 029 FR-11; plan "Missing characters and logs"): Debug/Development remember 128
// (font slot, code point) pairs and log each once (`text: U+4E00 not in fonts/body_16`), then one
// `text: more missing characters (log full)`. Release: nothing (an empty inline function).

#include <cstdint>

#include "nnh/build.hpp"

namespace nnh::detail {

inline constexpr int kMaxMissingLogged = 128;

/// Logs `cp` missing from the font in table slot `slot` (named `path`) unless already logged. Never allocates.
void text_missing_log(int slot, char32_t cp, const char* path);
/// Forgets what was logged (nnh::shutdown, tests).
void text_missing_reset();

inline void text_missing(int slot, char32_t cp, const char* path) {
    if constexpr (build::kDevToolsEnabled) text_missing_log(slot, cp, path);
}

} // namespace nnh::detail
