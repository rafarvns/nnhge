#pragma once
// Shared helpers for the sprite animation tests (spec 016).

#include <cstring>

#include "animation/animator_internal.hpp"
#include "crash/crash_internal.hpp"
#include "nnh/animation.hpp"
#include "nnh/app.hpp"
#include "nnh/build.hpp"
#include "nnh/platform_fake.hpp"
#include "test.hpp"

namespace anim_test {

/// Runs `fn` and reports whether it reached the crash screen (an assert). Recovers the module afterwards.
inline bool crashes(void (*fn)()) {
    nnh::detail::crash_install();
    const bool crashed = nnh::platform_fake::run_expecting_crash(fn);
    nnh::detail::animator_recover_after_crash();
    return crashed;
}

/// Misuse asserts in Debug/Development and is ignored (with a log) in Release.
inline bool asserts_or_is_ignored(void (*fn)()) {
    return crashes(fn) == nnh::build::kAssertsEnabled;
}

inline bool last_log_mentions(const char* text) {
    return std::strstr(nnh::platform_fake::state().last_console_line, text) != nullptr;
}

/// A fresh engine on the fake platform (console log lines).
inline void restart() {
    nnh::shutdown();
    nnh::platform_fake::reset();
    CHECK(nnh::init({}));
}

/// A library of `frames` 16×16 frames in one row (frame i at x = 16 i), with room for clips, markers and states.
inline nnh::SpriteLibrary strip(int frames = 8, nnh::SpritePivot pivot = nnh::SpritePivot::TopLeft) {
    auto made = nnh::SpriteLibrary::create({.frames = frames, .clips = 16, .clip_frames = 128, .markers = 64,
                                            .states = 8, .transitions = 16, .pivot = pivot});
    CHECK(made.ok());
    nnh::SpriteLibrary lib = std::move(made.value());
    CHECK(lib.add_grid({.frame_width = 16, .frame_height = 16, .count = frames, .columns = frames}).ok());
    return lib;
}

/// Adds a clip of frames [0, count) with `frames_each` 60 Hz frames per frame.
inline nnh::SpriteClipId clip(nnh::SpriteLibrary& lib, const char* name, int count, int frames_each,
                              nnh::SpriteMode mode = nnh::SpriteMode::Loop, int repeat = 0,
                              nnh::SpriteDirection direction = nnh::SpriteDirection::Forward) {
    auto made = lib.add_clip({.name = name, .count = count, .frame_time = nnh::Frames{frames_each}, .mode = mode,
                              .repeat = repeat, .direction = direction});
    CHECK(made.ok());
    return made.ok() ? made.value() : nnh::SpriteClipId{};
}

/// The positions shown after each of `n` updates.
template <int N>
void positions(nnh::SpriteAnimator& a, int (&out)[N]) {
    for (int i = 0; i < N; ++i) {
        a.update();
        out[i] = a.position();
    }
}

template <int N>
bool same(const int (&got)[N], const int (&want)[N]) {
    for (int i = 0; i < N; ++i) {
        if (got[i] != want[i]) return false;
    }
    return true;
}

inline int count_kind(nnh::SpriteNotices n, nnh::SpriteNoticeKind kind) {
    int c = 0;
    for (const nnh::SpriteNotice& x : n) c += x.kind == kind ? 1 : 0;
    return c;
}

} // namespace anim_test
