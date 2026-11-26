#pragma once
// Compiling CLDR plural rule text into the engine's bytecode (spec 029 FR-17; plan "Placeholders and plurals"):
// operands n and i (equal for integers), `%` by 10, 100, 1000 or 1000000, `=`/`!=`, values, ranges a..b and lists a,b,
// `and`, `or`; operands v, w, f, t, c and e are 0 for integers, so their relations are folded at compile time.
// Required categories: those the rule gives some integer in 0..1 000 999 or a power of ten up to 10^18, and Other.
// Tests: tests/test_plural_compile.cpp.

#include <cstdint>
#include <string>
#include <vector>

namespace nnh::assetc {

struct PluralSource {
    std::string category; ///< "zero", "one", "two", "few", "many"
    std::string rule;
};

/// @return "" (and the bytecode in `out`) or the problem.
std::string compile_plural(const std::vector<PluralSource>& rules, std::vector<std::uint8_t>& out);

/// The categories (bits 1 << loc::Category) `bytecode` can produce for integers, Other always included.
std::uint8_t required_categories(const std::vector<std::uint8_t>& bytecode);

/// "zero" … "other" → 0..5, else −1.
int category_index(const std::string& name);
const char* category_name(int index);

} // namespace nnh::assetc
