#pragma once
// Shared helpers for the camera rig tests (spec 018).

#include <bit>
#include <cstdint>
#include <cstring>

#include "crash/crash_internal.hpp"
#include "nnh/app.hpp"
#include "nnh/build.hpp"
#include "nnh/camera.hpp"
#include "nnh/platform_fake.hpp"
#include "test.hpp"

namespace cam_test {

/// Exactly (x, y), bit for bit equal as floats.
inline bool at(nnh::Vec2 v, float x, float y) {
    return v.x == x && v.y == y;
}

/// The same bits (tells +0 from -0, compares NaN).
inline bool same_bits(float a, float b) {
    return std::bit_cast<std::uint32_t>(a) == std::bit_cast<std::uint32_t>(b);
}
inline bool same_bits(nnh::Vec2 a, nnh::Vec2 b) {
    return same_bits(a.x, b.x) && same_bits(a.y, b.y);
}

/// Runs `n` updates towards `target`.
template <typename T>
void run(nnh::CameraRig& rig, T target, int n) {
    for (int i = 0; i < n; ++i) rig.update(target);
}

/// Runs `fn` and reports whether it reached the crash screen (an assert). Works with or without nnh::init().
inline bool crashes(void (*fn)()) {
    nnh::detail::crash_install(); // re-arms the crash path after an earlier expected crash
    return nnh::platform_fake::run_expecting_crash(fn);
}

/// Misuse asserts in Debug/Development and is handled (clamped/ignored, with a log) in Release (FR-15).
inline bool asserts_or_is_ignored(void (*fn)()) {
    return crashes(fn) == nnh::build::kAssertsEnabled;
}

/// A fresh engine on the fake platform.
inline void restart(const nnh::AppConfig& config = {}) {
    nnh::shutdown();
    nnh::platform_fake::reset();
    CHECK(nnh::init(config));
}

inline bool last_log_mentions(const char* text) {
    return std::strstr(nnh::platform_fake::state().last_console_line, text) != nullptr;
}

} // namespace cam_test
