#pragma once
// Helpers for the transition tests (spec 023): a fresh engine on the fake platform, frames driven like a game's loop,
// scenes that record when their callbacks run, crash and log helpers.

#include <cstdint>
#include <cstring>

#include "crash/crash_internal.hpp"
#include "nnh/app.hpp"
#include "nnh/build.hpp"
#include "nnh/gfx.hpp"
#include "nnh/log.hpp"
#include "nnh/platform_fake.hpp"
#include "nnh/scene.hpp"
#include "nnh/transition.hpp"
#include "test.hpp"

namespace tr_test {

namespace fake = nnh::platform_fake;
namespace tr = nnh::transition;

inline bool crashes(void (*fn)()) {
    nnh::detail::crash_install(); // re-arms the crash path after an earlier expected crash
    return fake::run_expecting_crash(fn);
}
/// Misuse asserts in Debug/Development and is handled (with a log) in Release (FR-22).
inline bool asserts_or_is_ignored(void (*fn)()) { return crashes(fn) == nnh::build::kAssertsEnabled; }

inline void restart(const nnh::AppConfig& config = {}) {
    nnh::shutdown();
    fake::reset();
    CHECK(nnh::init(config));
}

inline bool last_log_mentions(const char* text) {
    return std::strstr(fake::state().last_console_line, text) != nullptr;
}

inline std::uint32_t frame_no() { return static_cast<std::uint32_t>(nnh::frame_count()); }

/// One game frame: nnh::frame(), then the scene stack's step.
inline void step(int frames = 1) {
    for (int i = 0; i < frames; ++i) {
        CHECK(nnh::frame());
        nnh::scenes::update_and_draw();
    }
}

/// Steps until no transition runs; returns the frames stepped (-1 if it never ends).
inline int run_until_done(int max = 3000) {
    for (int i = 1; i <= max; ++i) {
        step();
        if (!tr::running()) return i;
    }
    return -1;
}

/// A scene that records the frames of its callbacks and draws its colour over both screens (layer 0) and a
/// small square on layer 15.
class Probe : public nnh::Scene {
public:
    explicit Probe(nnh::Color color = nnh::colors::red) : color_(color) {}

    void on_enter() override {
        ++enters;
        enter_frame = frame_no();
    }
    void on_exit() override {
        ++exits;
        exit_frame = frame_no();
    }
    void update() override {
        ++updates;
        if (first_update == 0) first_update = frame_no();
        last_update = frame_no();
        if (on_update != nullptr) on_update(*this);
    }
    void draw() override {
        ++draws;
        last_draw = frame_no();
        for (nnh::Screen s : {nnh::Screen::Top, nnh::Screen::Bottom}) {
            nnh::gfx::fill_rect(s, {0, 0, static_cast<float>(nnh::screen_width(s)), 240}, color_);
            nnh::gfx::fill_rect(s, {10, 10, 8, 8}, color_, {.layer = 15});
        }
    }

    int enters = 0, exits = 0, updates = 0, draws = 0;
    std::uint32_t enter_frame = 0, exit_frame = 0, first_update = 0, last_update = 0, last_draw = 0;
    void (*on_update)(Probe& self) = nullptr;
    void* user = nullptr;

private:
    nnh::Color color_;
};

/// Counts the log lines that contain `needle` while alive.
struct LogCount {
    const char* needle;
    int count = 0;
    explicit LogCount(const char* n) : needle(n) { nnh::log::add_sink(&LogCount::sink, this); }
    ~LogCount() { nnh::log::remove_sink(&LogCount::sink, this); }
    LogCount(const LogCount&) = delete;
    LogCount& operator=(const LogCount&) = delete;
    static void sink(nnh::LogLevel, const char* line, void* user) {
        auto* self = static_cast<LogCount*>(user);
        if (std::strstr(line, self->needle) != nullptr) ++self->count;
    }
};

} // namespace tr_test
