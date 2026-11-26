#pragma once
// What spans languages (spec 029 FR-18, FR-19, FR-21, FR-29, FR-31; plan "Build"): fallback chains (a shipped code,
// no cycle, ending at the base), key hash collisions across every language, font roles naming converted fonts, glyph
// coverage of each language's strings by its first role's font and that font's fallbacks, the pseudo-language `qps`
// (Debug/Development), and the manifest `.nnh_loc`. Tests: tests/test_strings_index.cpp.

#include <cstdint>
#include <string>
#include <vector>

#include "files.hpp"
#include "strings_check.hpp"
#include "strings_model.hpp"

namespace nnh::assetc {

/// The pseudo-language from the base: letters → Latin-1 look-alikes, "~" × ⌈0.3 × characters⌉, the whole in [ ].
StringsLanguage make_pseudo(const StringsLanguage& base);
std::string pseudo_text(const std::string& text);

struct IndexInput {
    std::vector<StringsLanguage> languages; ///< in the declared order (+ qps last when made)
    int base = 0;
    bool release = false;
    std::string fonts_dir; ///< where the converted fonts are (the game's output assets)
};

/// Checks across languages and writes the manifest into `out`.
void build_index(FileSource& files, const IndexInput& in, std::vector<std::uint8_t>& out, CheckResult& result);

} // namespace nnh::assetc
