#pragma once
// Shared helpers for the input action tests (spec 017).

#include "actions/actions_internal.hpp"
#include "crash/crash_internal.hpp"
#include "nnh/actions.hpp"
#include "nnh/app.hpp"
#include "nnh/build.hpp"
#include "nnh/platform_fake.hpp"
#include "test.hpp"

#include <cstring>

namespace actions_test {

using nnh::Button;
using nnh::Capability;

/// Every capability bit (a New 3DS).
inline constexpr std::uint8_t kAllCaps = 0x3F;

inline std::uint32_t bit(Button b) { return static_cast<std::uint32_t>(b); }

/// Runs `fn` and reports whether it reached the crash screen (an assert).
inline bool crashes(void (*fn)()) {
    nnh::detail::crash_install();
    return nnh::platform_fake::run_expecting_crash(fn);
}

/// Misuse asserts in Debug/Development and is ignored (with a log) in Release.
inline bool asserts_or_is_ignored(void (*fn)()) { return crashes(fn) == nnh::build::kAssertsEnabled; }

/// A fresh engine on the fake platform with every control (New 3DS) unless `old_3ds`.
inline void restart(bool old_3ds = false) {
    nnh::shutdown();
    nnh::platform_fake::reset();
    if (old_3ds) {
        nnh::platform_fake::set_capabilities(
            {Capability::SecondScreen, Capability::TouchScreen, Capability::CirclePad});
    } else {
        nnh::platform_fake::set_capabilities({Capability::SecondScreen, Capability::TouchScreen,
                                              Capability::CirclePad, Capability::CStick, Capability::ZButtons});
    }
    CHECK(nnh::init({}));
}

inline bool last_log_mentions(const char* text) {
    return std::strstr(nnh::platform_fake::state().last_console_line, text) != nullptr;
}

/// Sets the fake platform's controls for the next frame.
struct Pad {
    std::uint32_t buttons = 0;
    float cx = 0, cy = 0;  ///< Circle Pad (platform scale; the engine's 10 % dead zone applies)
    float sx = 0, sy = 0;  ///< C-Stick
    bool touch = false;
    int tx = 0, ty = 0;
};

inline void set(const Pad& p) {
    nnh::platform_fake::set_held_buttons(p.buttons);
    nnh::platform_fake::set_circle_pad(p.cx, p.cy);
    nnh::platform_fake::set_c_stick(p.sx, p.sy);
    if (p.touch) {
        nnh::platform_fake::set_touch(p.tx, p.ty);
    } else {
        nnh::platform_fake::release_touch();
    }
}

/// Runs one nnh::frame() with these controls (sets are evaluated inside it).
inline void frame(const Pad& p = {}) {
    set(p);
    CHECK(nnh::frame());
}

inline void frames(int n, const Pad& p = {}) {
    for (int i = 0; i < n; ++i) frame(p);
}

/// A pure view (no engine): sticks are taken as already shaped.
inline nnh::detail::InputView view(std::uint32_t held, nnh::Stick circle = {}, nnh::Stick c = {},
                                   bool touching = false, nnh::TouchPoint t = {}) {
    return nnh::detail::make_input_view(held, circle, c, touching, t);
}

/// A slot declared with every capability.
inline nnh::detail::SlotRec slot(const nnh::Binding& b, std::uint8_t caps = kAllCaps) {
    return nnh::detail::make_slot(b, caps);
}

inline bool near(float a, float b) { return a - b < 1e-5f && b - a < 1e-5f; }

/// The set's block (test-only: an ActionSet is exactly its block pointer).
inline nnh::detail::ActionBlock* block_of(const nnh::ActionSet& set) {
    static_assert(sizeof(nnh::ActionSet) == sizeof(nnh::detail::ActionBlock*));
    nnh::detail::ActionBlock* block = nullptr;
    std::memcpy(&block, &set, sizeof block);
    return block;
}

/// One evaluation of `set` with these readings, without the frame loop (pure stage 2 tests).
inline void step(nnh::ActionSet& set, std::uint32_t held, nnh::Stick circle = {}, nnh::Stick c = {},
                 bool touching = false, nnh::TouchPoint t = {}) {
    nnh::detail::evaluate(*block_of(set), view(held, circle, c, touching, t));
}

inline nnh::Binding btn(Button b) { return {nnh::Source::button(b)}; }

} // namespace actions_test
