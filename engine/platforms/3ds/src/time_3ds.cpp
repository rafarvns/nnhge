// Nintendo 3DS platform: monotonic clock from the ARM11 system tick (contract nnh/platform/time.hpp).

#include <3ds.h>

#include "nnh/platform/time.hpp"

namespace nnh::platform {

std::int64_t ticks_ns() {
    // svcGetSystemTick counts at SYSCLOCK_ARM11 (~268 MHz) on every model. Split the conversion so
    // ticks * 1e9 never overflows 64 bits (it would after ~69 s otherwise).
    constexpr std::uint64_t kHz = SYSCLOCK_ARM11;
    const std::uint64_t ticks = svcGetSystemTick();
    const std::uint64_t seconds = ticks / kHz;
    const std::uint64_t rest = ticks % kHz;
    return static_cast<std::int64_t>(seconds * 1'000'000'000ull + rest * 1'000'000'000ull / kHz);
}

} // namespace nnh::platform
