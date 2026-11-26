#pragma once
// The example's self-check (spec 029 AC-2): for each of en, pt-BR, de, ru, ja — switched like a player would, one
// frame each — a running FNV-1a over what the game shows: tr() of a fixed key list, format() of the shop lines (plural
// forms for 0, 1, 2, 5, 21, 1 000, −1, INT32_MAX; a price of INT64_MIN; a name), the dialog paragraph measured and laid
// out (measure_text, measure_chars) at 300 and 200 px, with a line limit and the ellipsis, centred and right-aligned, at
// scale 1 and 2, in the body and the title font, and the role fonts' metrics. Everything integer: the same sums on every
// system. Never the pseudo-language, so Debug and Release agree.

#include <cstdint>

namespace loc_check {

inline constexpr int kLanguages = 5;
inline constexpr const char* kCodes[kLanguages] = {"en", "pt-BR", "de", "ru", "ja"};

/// Per-language checkpoints and the final value, recorded from pc-debug (T7.2).
extern const std::uint32_t kCheckpoints[kLanguages];
extern const std::uint32_t kLocCheckGolden;

/// Folds the current language's sum into `hash` (localization started, the role fonts in use). Allocates nothing.
std::uint32_t language_sum(std::uint32_t hash);

} // namespace loc_check
