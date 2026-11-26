// live::pause() and live::show_panel() (spec 030 D52, FR-7): requests to the engine's pause (app/dev_pause.cpp) and to
// the overlay's pages (debug/overlay_pages.cpp).

#include "nnh/live.hpp"

namespace nnh::detail::live_api {

void pause() {}

void show_panel(std::uint8_t, const char*) {}

} // namespace nnh::detail::live_api
