#pragma once
// Drawing NNH Link's screens (spec 011 FR-2, FR-6) with the engine's text and shapes. Draw only: the
// state comes from LinkState; sizes and touch areas are in layout.hpp (shared with link_scene.cpp).

#include <string>

#include <nnh/gfx.hpp>

#include "layout.hpp"
#include "link_state.hpp"
#include "pad.hpp"

namespace link_app::view {

using layout::fit;
using layout::wrap;

inline constexpr nnh::Color kTopBackground = nnh::rgb(18, 22, 34);
inline constexpr nnh::Color kBottomBackground = nnh::rgb(26, 30, 42);
inline constexpr nnh::Color kAccent = nnh::rgb(90, 170, 255);
inline constexpr nnh::Color kDim = nnh::rgb(150, 156, 170);
inline constexpr nnh::Color kWarn = nnh::rgb(255, 120, 100);
inline constexpr nnh::Color kGood = nnh::rgb(120, 220, 140);

/// The title bar of the top screen, with `right` at its right end.
void header(const std::string& right);
/// Help line(s) at the bottom of the top screen.
void help(const char* text);
/// The button of layout::kButton* on the bottom screen.
void button(const char* label);

void draw_servers(const LinkState& link);             // view_lists.cpp
void draw_games(const LinkState& link);
void draw_pad(const LinkState& link, const Pad& pad); // view_pad.cpp (code and address)
void draw_progress(const LinkState& link);            // view_progress.cpp (connecting, building)
void draw_message(const LinkState& link);             // (message, done)

} // namespace link_app::view
