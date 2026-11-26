// live::pause() and live::show_panel() (spec 030 D52, FR-7): requests to the engine's pause (app/dev_pause.cpp) and to
// the overlay's pages (debug/overlay_pages.cpp).

#include "app/dev_pause.hpp"
#include "debug/overlay_pages.hpp"
#include "live/live_block.hpp"
#include "live/live_panel_model.hpp"
#include "nnh/live.hpp"

namespace nnh::detail::live_api {

void pause() { detail::dev_request_pause(); }

void show_panel(std::uint8_t page, const char* select) {
    detail::overlay_show_page(page);
    if (page != 0 && select != nullptr && detail::live::g_registry != nullptr) {
        detail::live::panel_select_name(*detail::live::g_registry, page, select);
    }
}

} // namespace nnh::detail::live_api
