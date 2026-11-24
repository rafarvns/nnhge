#pragma once
// The transitions example's self-check (spec 023 AC-3, AC-6): a fixed script of about 24 transitions over 1200 frames
// on the real engine — every built-in effect, both reveal styles, per-screen and tall-area variants, easings, a
// loading step of 7 slices with a custom effect, zero-length phases, a refused request and plain stack changes
// mid-way. Each frame, an FNV-1a checksum takes the frame number, phase(), both screens' coverage() bits,
// loading_progress() bits, the rectangles and colour transition::effect_rects/effect_color give for what each screen
// shows, and the frames of every on_enter, on_exit, update, on_covered, on_finished, custom and loading call. It must
// be the same on every compiler and on the 3DS. Engine calls only: the unit tests run it on the fake platform
// (tests/transitions_example/test_self_check.cpp), the example in the simulator and on the console.

#include <cstddef>
#include <cstdint>
#include <nnh/transition.hpp>

#include "check_scenes.hpp"

/// The checksum the simulator recorded for the whole script (pc-debug), and its value every 120 frames. A different
/// value on any system is a determinism hole (or a change of the script or the module: record the new values from
/// pc-debug, here and in examples/transitions/tests/check.nnhs).
inline constexpr std::uint32_t kTransitionsCheckGolden = 0xB32C7A79u; // pc-debug, WinLibs GCC 16
inline constexpr std::uint32_t kTransitionsCheckpoints[10] = {0x89F4F944u, 0x4BF8CFEEu, 0x58AEC725u, 0x8284B4EAu,
                                                              0xE271C1BBu, 0x70D4E35Eu, 0xC82DAF6Fu, 0xBF71A011u,
                                                              0x11D7022Eu, 0xB32C7A79u};

class TransitionsCheck {
public:
    static constexpr int kFrames = 1200;
    static constexpr int kCheckpointEvery = 120;
    enum class Note : std::uint8_t { Enter, Exit, Update, Covered, Finished, Custom, Loading, Refused };

    TransitionsCheck();
    TransitionsCheck(const TransitionsCheck&) = delete;
    TransitionsCheck& operator=(const TransitionsCheck&) = delete;

    /// Push driver() (or replace a scene with it) to start; it pushes A. Restarts the script.
    void reset();
    nnh::Scene& driver() { return driver_; }

    /// One frame of the script (the driver's draw). @return true when the frame is a checkpoint (every 120).
    bool tick();
    /// Mixes a scene's or a callback's call into the checksum, with the script's frame.
    void note(Note what, int id);

    bool done() const { return frame_ >= kFrames; }
    int frame() const { return frame_; }
    std::uint32_t checksum() const { return sum_; }
    /// The checkpoint sums so far (index = frame / 120 − 1).
    const std::uint32_t* checkpoints() const { return checkpoints_; }

    /// After the last frame: the scene to return to (the example's showcase). Null: the stack is left as it is.
    nnh::Scene* return_to = nullptr;
    /// Called at every checkpoint (the example logs it).
    void (*on_checkpoint)(const TransitionsCheck& check) = nullptr;

    CheckScene& scene(int id) { return id == 0 ? a_ : (id == 1 ? b_ : c_); }

private:
    void run_steps();
    void start(int op, nnh::Scene* target, const nnh::transition::Options& options);
    void mix(const void* data, std::size_t size);
    void mix_screen(nnh::Screen screen);

    CheckDriver driver_{*this};
    CheckScene a_, b_, c_;
    std::uint32_t sum_ = 0;
    std::uint32_t checkpoints_[kFrames / kCheckpointEvery] = {};
    int frame_ = 0;
    int top_ = 0;          ///< A (0) or B (1): the scene the next replace leaves
    int loading_calls_ = 0;
    nnh::transition::Options current_{}; ///< the last started transition's options (reveal_at applied)
};

/// "transitions: <frame> sum XXXXXXXX".
void format_transitions_checkpoint(char* out, std::size_t size, int frame, std::uint32_t sum);
/// "transitions: PASS XXXXXXXX", or "transitions: FAIL frame <f> expected XXXXXXXX got XXXXXXXX" (the first
/// checkpoint that differs).
void format_transitions_verdict(char* out, std::size_t size, const TransitionsCheck& check);
