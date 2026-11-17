#pragma once
// The interface every platform backend implements (engine/src/platform/<name>/).
// Core code talks to the hardware only through these functions.

#include <cstdint>

#include "nnh/app.hpp"
#include "nnh/gfx.hpp"

namespace nnh::platform {

bool init(const AppConfig& config);
void shutdown();

/// Processes system events. Returns false when the app must quit.
bool poll_events();

/// Buttons currently down, as a mask of nnh::Button bits.
std::uint32_t read_buttons();

/// Starts a frame with both screens cleared.
void begin_frame(Color top_clear, Color bottom_clear);

/// Submits the frame and waits for vsync.
void end_frame();

void debug_text(Screen screen, int x, int y, const char* text, Color color);
int debug_line_height();

} // namespace nnh::platform
