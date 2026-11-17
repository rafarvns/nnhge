#pragma once
// Shared helpers for the event bus tests: a fresh engine on the fake platform and a few event types.

#include "nnh/app.hpp"
#include "nnh/events.hpp"
#include "nnh/platform_fake.hpp"
#include "test.hpp"

namespace bus_test {

struct Ping {
    int value;
};
struct Pong {
    int value;
};

/// Shuts down any previous engine, resets the fake platform and starts the engine with `config`.
inline void restart(const nnh::AppConfig& config = {}) {
    nnh::shutdown();
    nnh::platform_fake::reset();
    CHECK(nnh::init(config));
}

} // namespace bus_test
