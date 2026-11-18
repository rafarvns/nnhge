#pragma once
// PC simulator: the window settings remembered between runs (spec 006), as text lines `key=value`.
// Pure and SDL-free (tests/simulator/test_sim_settings.cpp). The file comes from the user's disk, so
// parsing is bounded and validates every value.

#include <cstddef>

#include "sim_layout.hpp"

namespace nnh::platform::pc {

inline constexpr std::size_t kMaxSettingsBytes = 1024;

struct SimSettings {
    SimLayout layout = SimLayout::Stacked;
    int scale = 2;         ///< 1..kMaxSimScale
    bool fit = false;
    bool fullscreen = false;
    bool has_window = false; ///< x, y, w, h below are valid
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

/// Reads `size` bytes of `text` (no terminator needed) over `out`: known keys with valid values replace
/// the defaults, anything else is ignored. @return false if `text` is null or larger than
/// kMaxSettingsBytes (then `out` is unchanged).
bool parse_settings(const char* text, std::size_t size, SimSettings& out);

/// Writes `settings` as text. @return its length, or -1 if `size` is too small.
int format_settings(const SimSettings& settings, char* out, std::size_t size);

} // namespace nnh::platform::pc
