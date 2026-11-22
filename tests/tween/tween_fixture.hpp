#pragma once
// Shared helpers for the tween tests (spec 015).

#include "crash/crash_internal.hpp"
#include "nnh/app.hpp"
#include "nnh/build.hpp"
#include "nnh/platform_fake.hpp"
#include "nnh/tween.hpp"
#include "test.hpp"
#include "tween/tween_internal.hpp"

#include <cstring>

namespace tween_test {

/// Runs `fn` and reports whether it reached the crash screen (an assert). Works with or without nnh::init().
inline bool crashes(void (*fn)()) {
    nnh::detail::crash_install(); // re-arms the crash path after an earlier expected crash
    return nnh::platform_fake::run_expecting_crash(fn);
}

/// Misuse asserts in Debug/Development and is ignored (with a log) in Release (spec 015 FR-18).
inline bool asserts_or_is_ignored(void (*fn)()) {
    return crashes(fn) == nnh::build::kAssertsEnabled;
}

/// A fresh engine on the fake platform (event delivery, console log lines).
inline void restart(const nnh::AppConfig& config = {}) {
    nnh::shutdown();
    nnh::platform_fake::reset();
    CHECK(nnh::init(config));
}

inline bool last_log_mentions(const char* text) {
    return std::strstr(nnh::platform_fake::state().last_console_line, text) != nullptr;
}

/// After an expected assert inside an update or a callback (the crash path jumps out of it), marks the set
/// idle again so it can be used and destroyed. Test-only: a TweenSet is exactly its block pointer.
inline void recover_after_crash(nnh::TweenSet& set) {
    static_assert(sizeof(nnh::TweenSet) == sizeof(nnh::detail::TweenBlock*));
    nnh::detail::TweenBlock* block = nullptr;
    std::memcpy(&block, &set, sizeof block);
    if (block == nullptr) return;
    block->busy = 0;
    block->callback_depth = 0;
    nnh::detail::tween_sweep(*block);
}

/// Runs `n` updates of `set`.
inline void run(nnh::TweenSet& set, int n) {
    for (int i = 0; i < n; ++i) {
        set.update();
    }
}

} // namespace tween_test
