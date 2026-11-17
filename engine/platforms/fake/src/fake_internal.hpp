#pragma once
// Mutable state shared by the fake platform files. Internal to engine/platforms/fake/.

#include <cstddef>
#include <cstdint>

#include "nnh/memory.hpp"

#include "nnh/platform_fake.hpp"

namespace nnh::platform_fake::detail {

struct Controls {
    std::uint32_t held_buttons = 0;
    bool quit_requested = false;
    bool init_fails = false;
    std::int64_t time_ns = 0; // manual clock read by ticks_ns()
    std::size_t region_capacity[kMemRegionCount] = {64u << 20, 32u << 20, 6u << 20};
    std::size_t region_used[kMemRegionCount] = {};
};

State& mutable_state();
Controls& controls();
void fs_reset(); // fs_fake.cpp

} // namespace nnh::platform_fake::detail
