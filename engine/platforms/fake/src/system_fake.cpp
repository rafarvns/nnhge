// Fake platform: configurable capabilities and developer requests (contract nnh/platform/system.hpp).

#include "fake_internal.hpp"
#include "nnh/platform/system.hpp"

namespace nnh {

std::uint32_t platform::capabilities() { return platform_fake::detail::controls().capabilities; }

SystemInfo platform::system_info() { return SystemInfo{"Fake platform", "unit tests", 64}; }

std::uint32_t platform::poll_dev_requests() {
    auto& c = platform_fake::detail::controls();
    std::uint32_t requests = c.dev_requests;
    c.dev_requests = 0;
    if (c.dev_queue_next < c.dev_queue_count) requests |= c.dev_queue[c.dev_queue_next++];
    return requests;
}

void platform_fake::set_capabilities(std::initializer_list<Capability> capabilities) {
    std::uint32_t mask = 0;
    for (Capability c : capabilities) mask |= 1u << static_cast<unsigned>(c);
    detail::controls().capabilities = mask;
}

void platform_fake::request_overlay_toggle() { detail::controls().dev_requests |= platform::kDevToggleOverlay; }
void platform_fake::request_pause() { detail::controls().dev_requests |= platform::kDevTogglePause; }
void platform_fake::request_step() { detail::controls().dev_requests |= platform::kDevStepFrame; }
void platform_fake::request_hold() { detail::controls().dev_requests |= platform::kDevHoldAtStart; }

void platform_fake::queue_dev_requests(std::initializer_list<std::uint32_t> masks) {
    auto& c = detail::controls();
    c.dev_queue_count = 0;
    c.dev_queue_next = 0;
    for (std::uint32_t m : masks) {
        if (c.dev_queue_count < 64) c.dev_queue[c.dev_queue_count++] = m;
    }
}

// Reported as set, even without Stereo3D (core applies the capability and the clamp).
float platform::stereo_slider() { return platform_fake::detail::controls().stereo_slider; }

void platform_fake::set_stereo_slider(float value) { detail::controls().stereo_slider = value; }

} // namespace nnh
