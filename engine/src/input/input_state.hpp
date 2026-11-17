#pragma once
// Platform-independent button edge detection. Pure logic, unit tested in tests/test_input.cpp.

#include <cstdint>

namespace nnh::detail {

struct InputState {
    std::uint32_t held = 0;
    std::uint32_t previous = 0;

    void update(std::uint32_t now) {
        previous = held;
        held = now;
    }
    std::uint32_t pressed() const { return held & ~previous; }
    std::uint32_t released() const { return previous & ~held; }
};

} // namespace nnh::detail
