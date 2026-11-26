// The self-check's 3600-frame script (spec 028 AC-2): segments of held controls and touches played one after another,
// then repeated with variations until the end. Each control segment presses on its first frame and holds for the rest;
// each touch segment starts on its first frame, slides linearly to its end point and releases on its last frame.

#include "self_check.hpp"

namespace ui_check {
namespace {

constexpr std::uint8_t U = 1, D = 2, L = 4, R = 8, A = 16, B = 32, PB = 64, PF = 128;

struct Segment {
    std::int16_t frames;
    std::uint8_t held;      ///< controls (0 with x < 0: idle)
    std::int16_t x, y;      ///< a touch from (x, y) ... (x < 0: none)
    std::int16_t x2, y2;    ///< ... to (x2, y2)
};

constexpr Segment c(std::uint8_t held, int frames = 2) { return {static_cast<std::int16_t>(frames), held, -1, -1, -1, -1}; }
constexpr Segment idle(int frames) { return c(0, frames); }
constexpr Segment touch(int x, int y, int frames = 4) {
    return {static_cast<std::int16_t>(frames), 0, static_cast<std::int16_t>(x), static_cast<std::int16_t>(y),
            static_cast<std::int16_t>(x), static_cast<std::int16_t>(y)};
}
constexpr Segment drag(int x, int y, int x2, int y2, int frames) {
    return {static_cast<std::int16_t>(frames), 0, static_cast<std::int16_t>(x), static_cast<std::int16_t>(y),
            static_cast<std::int16_t>(x2), static_cast<std::int16_t>(y2)};
}

// One pass over the screens (about 900 frames); played four times with the frame number mixed into a few holds.
constexpr Segment kPass[] = {
    idle(10),
    // title: wrap on a fresh press, the disabled Continue skipped, Extras refused, up into the top root's Info
    c(U), idle(6), c(D), idle(6), c(D), idle(6), c(D), idle(6), c(D), idle(6), c(A), idle(6), c(U, 40), idle(6),
    c(U), idle(6), c(D), idle(6),
    // options: sliders by direction and repeat, choices, the toggle, the disabled entry skipped, Back by cancel
    c(D), idle(6), c(A), idle(8), drag(150, 52, 230, 52, 16), idle(6), c(L, 40), idle(6), c(R), idle(6), c(D), idle(6), c(R, 70), idle(6), c(D), idle(6),
    c(A), idle(6), c(D), idle(6), c(L), idle(6), c(L), idle(6), c(D), idle(6), c(R), idle(6), c(R), idle(6),
    c(D, 60), idle(6), c(U), idle(6), drag(150, 60, 220, 60, 20), idle(6), drag(160, 200, 160, 120, 24), idle(6),
    c(B), idle(10),
    // the list: holds, pages, a disabled row refused or skipped, taps, drags past the threshold, a slide-out
    c(D), idle(6), c(A), idle(8), c(D, 120), idle(6), c(PF), idle(6), c(PF, 50), idle(6), c(PB), idle(6), c(A), idle(6),
    touch(100, 60), idle(6), touch(100, 60), idle(6), drag(100, 150, 100, 60, 18), idle(6),
    drag(100, 100, 100, 108, 10), idle(6), drag(100, 80, 300, 80, 12), idle(6), c(U, 30), idle(6), c(B), idle(10),
    // the grid: four directions, wraps, a tap
    c(D), idle(6), c(A), idle(8), c(R), idle(6), c(R, 40), idle(6), c(D), idle(6), c(D, 40), idle(6), c(L), idle(6),
    c(U), idle(6), touch(60, 80), idle(6), drag(160, 160, 160, 40, 20), idle(6), c(B), idle(10),
    // the dialog: reveal, show all, close
    c(U, 40), idle(6), c(A), idle(60), c(A), idle(8), c(A), idle(10),
    // the quit prompt: No by cancel, a touch outside ignored, Yes by touch
    c(D, 120), idle(6), c(A), idle(10), c(B), idle(8), c(A), idle(10), touch(5, 5), idle(6), c(L), idle(6), c(A),
    idle(10),
    // a mixed tail: several controls at once, a held confirm, opposite directions
    c(U | D), idle(4), c(L | R | A), idle(4), c(A, 30), idle(6), c(D | PF), idle(4), c(B), idle(10),
};
constexpr int kSegments = sizeof kPass / sizeof kPass[0];

constexpr int pass_frames() {
    int n = 0;
    for (const Segment& s : kPass) n += s.frames;
    return n;
}

} // namespace

nnh::UiInput script_input(int frame) {
    constexpr int kPassFrames = pass_frames();
    const int pass = frame / kPassFrames;
    int f = frame % kPassFrames;
    int i = 0;
    while (i < kSegments && f >= kPass[i].frames) f -= kPass[i++].frames;
    nnh::UiInput in;
    if (i >= kSegments) return in;
    const Segment& s = kPass[i];
    if (s.x < 0) {
        // odd passes turn the held directions round (Up → Down → Left → Right → Up) so they cover other paths
        std::uint8_t held = s.held;
        if (pass % 2 == 1 && (held & 0x0F) != 0) {
            const std::uint8_t d = held & 0x0F;
            held = static_cast<std::uint8_t>((held & 0xF0) | ((d << 1) & 0x0F) | ((d & 8) != 0 ? 1 : 0));
        }
        in.held = held;
        in.pressed = f == 0 ? held : 0;
        return in;
    }
    const int last = s.frames - 1;
    in.touch_x = static_cast<std::int16_t>(s.x + (s.x2 - s.x) * f / (last > 0 ? last : 1) + pass * 3);
    in.touch_y = static_cast<std::int16_t>(s.y + (s.y2 - s.y) * f / (last > 0 ? last : 1));
    in.touching = f < last;
    in.touch_started = f == 0;
    in.touch_ended = f == last;
    return in;
}

} // namespace ui_check
