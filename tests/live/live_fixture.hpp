#pragma once
// Shared helpers for the live value tests (spec 030): a fresh engine on the fake platform, misuse runs, frames.

#include <cstdint>

#include "crash/crash_internal.hpp"
#include "nnh/app.hpp"
#include "nnh/build.hpp"
#include "nnh/live.hpp"
#include "nnh/platform_fake.hpp"
#include "test.hpp"

namespace live_test {

namespace fake = nnh::platform_fake;

/// A fresh engine on the fake platform (the registry of the previous one freed by its shutdown hook).
inline void restart() {
    nnh::shutdown();
    fake::reset();
    CHECK(nnh::init({}));
}

/// Runs `fn` and reports whether it reached the crash screen (an assert).
inline bool crashes(void (*fn)()) {
    nnh::detail::crash_install();
    return fake::run_expecting_crash(fn);
}

/// Misuse asserts in Debug/Development and is harmless in Release.
inline bool asserts_or_is_ignored(void (*fn)()) { return crashes(fn) == nnh::build::kAssertsEnabled; }

/// `n` engine frames.
inline void frames(int n) {
    for (int i = 0; i < n; ++i) CHECK(nnh::frame());
}

inline std::uint32_t frame_now() { return static_cast<std::uint32_t>(nnh::frame_count()); }

} // namespace live_test
