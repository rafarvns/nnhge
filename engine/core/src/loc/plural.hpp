#pragma once
// Plural rules as bytecode (spec 029 FR-17, D50; plan "Placeholders and plurals"): nnh-assetc compiles CLDR integer
// rule text (plural_compile.cpp) into this form, the manifest carries it, the console only interprets it. Pure, no
// engine includes, no division by a variable (moduli 10, 100, 1000 and 1 000 000 are constants).
//
//   rule := count u8, then `count` times: category u8, branches u8, then per branch (or): conditions u8, then per
//           condition (and): op u8 = modulus << 1 | negate, ranges u8, then ranges × (lo u32, hi u32) little-endian
//   modulus: 0 none, 1 %10, 2 %100, 3 %1000, 4 %1000000
// A category whose branch holds is the answer (categories in their order); none: Other.
// Tests: tests/loc/test_plural_rules.cpp.

#include <cstddef>
#include <cstdint>

#include "loc/loc_file.hpp"

namespace nnh::detail::loc {

inline constexpr std::size_t kMaxRuleBytes = 255;

/// The category of the integer `n` (its absolute value) under `rule` (`bytes` 0: Other). The rule must be valid.
Category plural_category(const std::uint8_t* rule, std::size_t bytes, std::uint64_t n);

/// The rule is well-formed: every count within `bytes`, categories Zero..Many in increasing order, moduli 0..4,
/// lo ≤ hi. Bounded; never allocates.
bool plural_valid(const std::uint8_t* rule, std::size_t bytes);

} // namespace nnh::detail::loc
