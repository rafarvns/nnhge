#pragma once
// The tweens example's self-check (spec 015 AC-5): a fixed script of tweens, setters, sequences, loops,
// yoyos, speeds, timers in a set and nnh::Timer values, played for 600 frames. An FNV-1a checksum takes every
// value's bits and the order of every callback; the result must be the same bits on PC and 3DS. Pure code (no
// drawing, no input), so the unit tests run it too (tests/tweens/test_self_check.cpp).

#include <cstddef>
#include <cstdint>
#include <nnh/gfx.hpp>
#include <nnh/tween.hpp>

/// The checksum the simulator recorded for the whole script. A different value on any system is a determinism
/// hole (or a change of the script or of the tween module: then record the new value from pc-debug).
inline constexpr std::uint32_t kSelfCheckGolden = 0xB44CA3F6u; // pc-debug and pc-release, WinLibs GCC 16

class SelfCheck {
public:
    static constexpr int kFrames = 600;
    static constexpr int kCheckpointEvery = 60;

    SelfCheck(); ///< starts the script (frame 0)
    SelfCheck(const SelfCheck&) = delete;
    SelfCheck& operator=(const SelfCheck&) = delete;

    /// Plays one frame. @return true when that frame is a checkpoint (every 60 frames, the last included).
    bool update();
    bool done() const { return frame_ >= kFrames; }
    int frame() const { return frame_; }
    std::uint32_t checksum() const { return sum_; }

private:
    void mix(std::uint32_t word);
    void mix(float value);
    void record(char what); ///< a callback ran: its letter goes into the checksum, in order
    void script_frame();    ///< commands the script gives at fixed frames

    nnh::TweenSet set_{{.tweens = 32, .steps = 16}};
    nnh::TweenHandle forever_{};
    nnh::Timer shots_ = nnh::Timer::repeating(nnh::Frames{17});
    nnh::Timer once_ = nnh::Timer::once(nnh::Duration::from_ms(2500));
    float a_ = 0, setter_value_ = 0, s1_ = 0, s2_ = 0;
    nnh::Vec2 v_{3, 4}, v2_{};
    nnh::Color c_{};
    std::uint32_t sum_;
    int frame_ = 0;
};

/// `1 + 1.5·2⁻²⁴` computed at run time must round up (round to nearest), as libctru and x86-64 do.
bool rounding_probe_ok();

/// "tweens: <frame> sum XXXXXXXX" (a checkpoint log line).
void format_checkpoint(char* out, std::size_t size, int frame, std::uint32_t sum);
/// "tweens: PASS XXXXXXXX", or "tweens: FAIL sum XXXXXXXX expected XXXXXXXX rounding ok|WRONG".
void format_verdict(char* out, std::size_t size, std::uint32_t sum, bool rounding_ok);
