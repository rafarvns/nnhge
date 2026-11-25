#pragma once
// PC simulator: the window settings remembered between runs (spec 006), as text lines `key=value`.
// Pure and SDL-free (tests/simulator/test_sim_settings.cpp). The file comes from the user's disk, so
// parsing is bounded and validates every value.

#include <cstddef>
#include <cstdint>

#include "sim_layout.hpp"

namespace nnh::platform::pc {

inline constexpr std::size_t kMaxSettingsBytes = 1024;

/// What the top screen shows when a frame drew both eyes (spec 026 FR-22, F8).
enum class SimEye : std::uint8_t { Left, Right, Anaglyph };

struct SimSettings {
    SimLayout layout = SimLayout::Stacked;
    int scale = 2;         ///< 1..kMaxSimScale
    bool fit = false;
    bool fullscreen = false;
    int slider = 0;        ///< the 3D slider in hundredths, 0..100 (spec 026 FR-16)
    SimEye eye = SimEye::Left;
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

/// "0", "1", "0.5", "0.35": a slider value 0..1 with at most two decimals, as hundredths. @return false (and leaves
/// `hundredths`) otherwise (NNH_SIM_SLIDER, window.ini).
bool parse_slider(const char* text, int& hundredths);
/// The slider moved by `delta` hundredths, kept in 0..100 (F6/F7: ±10, with Shift ±1).
int step_slider(int hundredths, int delta);
/// Left → Right → Anaglyph → Left (F8).
SimEye next_eye(SimEye eye);
/// "left", "right", "anaglyph" (NNH_SIM_EYE, window.ini).
const char* eye_id(SimEye eye);
/// Parses an eye_id(). @return false (and leaves `out`) if unknown.
bool parse_eye(const char* id, SimEye& out);

} // namespace nnh::platform::pc
