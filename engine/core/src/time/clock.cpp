// Engine clock and frame delta (nnh/time.hpp).

#include "nnh/build.hpp"
#include "nnh/platform/time.hpp"
#include "nnh/time.hpp"
#include "time/time_internal.hpp"

namespace nnh {
namespace {

bool g_started = false;
std::int64_t g_origin_ns = 0;
std::int64_t g_last_frame_ns = 0;
bool g_has_frame = false;
Duration g_delta{};

} // namespace

void detail::time_reset(std::int64_t platform_ns) {
    g_started = true;
    g_origin_ns = platform_ns;
    g_has_frame = false;
    g_delta = {};
}

void detail::time_frame_started(std::int64_t platform_ns) {
    if constexpr (build::kDevToolsEnabled) {
        if (g_time_keep_delta) { // after a pause (spec 030): the clock restarts now, the delta stays
            g_time_keep_delta = false;
            g_last_frame_ns = platform_ns;
            return;
        }
    }
    g_delta = g_has_frame ? Duration{platform_ns - g_last_frame_ns} : Duration{};
    g_last_frame_ns = platform_ns;
    g_has_frame = true;
}

Duration time::now() { return g_started ? Duration{platform::ticks_ns() - g_origin_ns} : Duration{}; }

Duration time::delta() { return g_delta; }

} // namespace nnh
