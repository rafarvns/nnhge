#pragma once
// The animation example's self-check (spec 016 AC-6): 64 animators on the gallery and knight libraries (built in
// code) follow a fixed script for 600 frames: every clip mode, queues, speeds 0/0.5/1.5, set_frame, advance() by
// FixedStep-sized spans, state requests, forces, keep-time switches and callbacks that change clips. An FNV-1a
// checksum takes every shown frame and every notice; it must be the same on PC and 3DS. Pure code (no drawing, no
// input, no assets), so the unit tests run it too (tests/animation_example/test_self_check.cpp).

#include <cstddef>
#include <cstdint>
#include <nnh/animation.hpp>

#include "libraries.hpp"

/// The checksum the simulator recorded for the whole script. Another value on any system is a determinism hole (or
/// a change of the script or of the module: then record the new value from pc-debug).
inline constexpr std::uint32_t kAnimationSelfCheckGolden = 0x512F027Fu; // pc-debug, WinLibs GCC 16

class AnimationSelfCheck {
public:
    static constexpr int kFrames = 600;
    static constexpr int kCheckpointEvery = 60;
    static constexpr int kAnimators = 64;

    AnimationSelfCheck(); ///< builds the libraries and starts the script (frame 0)
    AnimationSelfCheck(const AnimationSelfCheck&) = delete;
    AnimationSelfCheck& operator=(const AnimationSelfCheck&) = delete;

    /// Plays one frame. @return true when that frame is a checkpoint (every 60 frames, the last included).
    bool update();
    bool done() const { return frame_ >= kFrames; }
    int frame() const { return frame_; }
    std::uint32_t checksum() const { return sum_; }
    int notices() const { return notices_; } ///< seen so far (all kinds)

private:
    void mix(std::uint32_t word);
    void start(int i);
    void script(int i);
    void request(int i);

    nnh::SpriteLibrary gallery_ = make_gallery_library();
    nnh::SpriteLibrary knight_ = make_knight_library();
    KnightStates states_{knight_};
    nnh::SpriteAnimator animators_[kAnimators];
    std::uint32_t sum_;
    int frame_ = 0;
    int notices_ = 0;
};

/// "animation: <frame> sum XXXXXXXX" (a checkpoint log line).
void format_animation_checkpoint(char* out, std::size_t size, int frame, std::uint32_t sum);
/// "animation: PASS XXXXXXXX", or "animation: FAIL <frame> XXXXXXXX XXXXXXXX" (got, want).
void format_animation_verdict(char* out, std::size_t size, int frame, std::uint32_t sum);
