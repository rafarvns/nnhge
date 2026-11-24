#pragma once
// The top screen's text of the saves example (spec 027 AC-5): the game's values, the settings, and the last
// load report (backup used, the version that wrote the file, steps run, unknown / dropped / adjusted counts).

#include "progress.hpp"

#include <cstddef>
#include <nnh/saves.hpp>

namespace saves_example {

/// Two lines: "v1 -> v3  steps 2  backup used", then "unknown 0  dropped 0  adjusted 0".
void format_report(char* out, std::size_t capacity, const nnh::save::Report& report);

void draw_hud(const Progress& p, int volume, const char* language, const char* last_report);

} // namespace saves_example
