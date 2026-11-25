#pragma once
// Shared helpers for the UI tests (spec 028): creating UIs, misuse runs, one frame of input, the engine on the fake
// platform, the fake's recorded items.

#include <bit>
#include <cstdint>
#include <cstring>

#include "crash/crash_internal.hpp"
#include "nnh/app.hpp"
#include "nnh/build.hpp"
#include "nnh/platform_fake.hpp"
#include "nnh/ui.hpp"
#include "test.hpp"

namespace ui_test {

namespace fake = nnh::platform_fake;

/// The game's keys in the tests.
enum class K { A, B, C, D, E, F, G, H, Back, Yes, No, List, Slider, Toggle, Choice };

/// A UI that must be created.
inline nnh::Ui make(const nnh::UiDesc& d = {}) {
    auto r = nnh::Ui::create(d);
    CHECK(r.ok());
    return r ? static_cast<nnh::Ui&&>(r.value()) : nnh::Ui{};
}

/// Runs `fn` and reports whether it reached the crash screen (an assert).
inline bool crashes(void (*fn)()) {
    nnh::detail::crash_install();
    return fake::run_expecting_crash(fn);
}

/// Misuse asserts in Debug/Development and is harmless in Release.
inline bool asserts_or_is_ignored(void (*fn)()) { return crashes(fn) == nnh::build::kAssertsEnabled; }

/// A fresh engine on the fake platform.
inline void restart(const nnh::AppConfig& config = {}) {
    nnh::shutdown();
    fake::reset();
    CHECK(nnh::init(config));
}

/// One frame of input with `controls` freshly pressed (and held).
inline nnh::UiInput press(std::uint8_t controls) {
    nnh::UiInput in;
    in.held = controls;
    in.pressed = controls;
    return in;
}
/// Held without a new press.
inline nnh::UiInput hold(std::uint8_t controls) {
    nnh::UiInput in;
    in.held = controls;
    return in;
}
inline nnh::UiInput touch_start(int x, int y) {
    nnh::UiInput in;
    in.touching = in.touch_started = true;
    in.touch_x = static_cast<std::int16_t>(x);
    in.touch_y = static_cast<std::int16_t>(y);
    return in;
}
inline nnh::UiInput touch_hold(int x, int y) {
    nnh::UiInput in;
    in.touching = true;
    in.touch_x = static_cast<std::int16_t>(x);
    in.touch_y = static_cast<std::int16_t>(y);
    return in;
}
inline nnh::UiInput touch_end(int x, int y) {
    nnh::UiInput in;
    in.touch_ended = true;
    in.touch_x = static_cast<std::int16_t>(x);
    in.touch_y = static_cast<std::int16_t>(y);
    return in;
}

constexpr std::uint8_t kUp = nnh::ui_bit(nnh::UiControl::Up);
constexpr std::uint8_t kDown = nnh::ui_bit(nnh::UiControl::Down);
constexpr std::uint8_t kLeft = nnh::ui_bit(nnh::UiControl::Left);
constexpr std::uint8_t kRight = nnh::ui_bit(nnh::UiControl::Right);
constexpr std::uint8_t kConfirm = nnh::ui_bit(nnh::UiControl::Confirm);
constexpr std::uint8_t kCancel = nnh::ui_bit(nnh::UiControl::Cancel);
constexpr std::uint8_t kPageBack = nnh::ui_bit(nnh::UiControl::PageBack);
constexpr std::uint8_t kPageForward = nnh::ui_bit(nnh::UiControl::PageForward);

/// A release then a fresh press (two updates; the results are the press's). The first update of a UI is an
/// activation, so tests start with `settle(ui)` (one idle update).
inline void tap(nnh::Ui& ui, std::uint8_t controls) {
    ui.update(nnh::UiInput{});
    ui.update(press(controls));
}
inline void settle(nnh::Ui& ui) { ui.update(nnh::UiInput{}); }

/// Results of the last update of a kind.
inline int count(const nnh::Ui& ui, nnh::UiResultKind kind) {
    int n = 0;
    for (const nnh::UiResult& r : ui.results()) n += r.kind == kind ? 1 : 0;
    return n;
}
inline bool has(const nnh::Ui& ui, nnh::UiResultKind kind) { return count(ui, kind) > 0; }

inline std::uint32_t bits(float f) { return std::bit_cast<std::uint32_t>(f); }

} // namespace ui_test
