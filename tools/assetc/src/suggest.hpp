#pragma once
// "Did you mean ...?" for nnh-assetc's messages (spec 014 FR-7, FR-8): unknown fields, enum names and
// referenced data assets suggest the closest declared name. Tests: tools/assetc/tests/test_suggest.cpp.

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace nnh::assetc {

/// Levenshtein distance (insertions, deletions, substitutions). Names here are short (at most 255 bytes).
std::size_t edit_distance(std::string_view a, std::string_view b);

/// The name of `names` nearest to `typed`, if it is at most max(1, typed.size() / 3) edits away (the first
/// one on a tie), else "".
std::string closest(std::string_view typed, const std::vector<std::string>& names);

} // namespace nnh::assetc
