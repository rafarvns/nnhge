#pragma once
// Mutable state shared by the fake platform files. Internal to engine/platforms/fake/.

#include <cstdint>

#include "nnh/platform_fake.hpp"

namespace nnh::platform_fake::detail {

struct Controls {
    std::uint32_t held_buttons = 0;
    bool quit_requested = false;
    bool init_fails = false;
};

State& mutable_state();
Controls& controls();

} // namespace nnh::platform_fake::detail
