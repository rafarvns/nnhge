#pragma once
// Asset paths and load names for hot reload (spec 031 FR-13, FR-15, plan question 7). Pure: shared with tools/nnh.
// A load name is an asset path without its last extension ("art/hero.png" → "art/hero"); a file whose last segment
// starts with its only dot (".nnh_assets") has no extension. Tests: tests/reload/test_reload_names.cpp.

#include <cstddef>

namespace nnh::detail::reload {

/// Characters the log line's list may take (FR-15).
inline constexpr std::size_t kListLimit = 160;

/// Length of `path`'s load name (the whole path when it has no extension).
std::size_t load_name_length(const char* path);

/// True when `path_or_name` equals `path` or `path`'s load name. Null strings never match.
bool matches(const char* path_or_name, const char* path);

/// The load names of `paths` in order, joined by ", ", into `out` (`capacity` ≥ kListLimit + 1). When they don't fit in
/// kListLimit characters the list is cut after the last name that leaves room for ", ... (+N)" (N names left out).
/// @return the length written. Never allocates.
std::size_t format_list(const char* const* paths, int count, char* out, std::size_t capacity);

} // namespace nnh::detail::reload
