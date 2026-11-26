#pragma once
// Pause and step in core (spec 030 D52, FR-16): Debug/Development only. PauseState is the pure state machine (tested
// alone in tests/app/test_dev_pause_state.cpp); dev_frame_begin() runs the paused ticks inside nnh::frame()
// (tests/app/test_dev_pause.cpp).

#include <cstdint>

namespace nnh::platform {
struct InputSample;
}

namespace nnh::detail {

/// Running, paused, stepping (n game frames left) or held at the start (waiting for a PC, 600 ticks at most).
class PauseState {
public:
    static constexpr int kHoldTicks = 600; ///< 10 s at 60 Hz

    /// Requests; each takes effect at the next frame start (begin_frame()).
    void pause();
    void resume();
    void toggle(); ///< SELECT + Y, the simulator's Pause key
    /// Runs `frames` (>= 1) game frames, then pauses again. A step while running pauses first.
    void step(std::uint16_t frames);
    /// The held start: paused before frame 1 until a resume or step, or kHoldTicks paused ticks without a PC.
    void hold();

    /// At a frame start: true runs a game frame now (one step consumed); false means a paused tick.
    bool begin_frame();
    /// A paused tick passed (`attached`: a PC is attached). @return true when a held start just timed out.
    bool tick(bool attached);

    /// The next frame start is a paused tick (a step's frames are not paused).
    bool paused() const { return mode_ == Mode::Paused; }
    bool held() const { return held_; }
    std::uint16_t steps_left() const { return mode_ == Mode::Stepping ? steps_ : 0; }

private:
    enum class Mode : std::uint8_t { Running, Paused, Stepping };
    Mode mode_ = Mode::Running;
    std::uint16_t steps_ = 0;
    int held_ticks_ = 0;
    bool held_ = false;
};

/// The engine's pause state (dev_pause.cpp).
PauseState& dev_pause_state();

/// The Debug/Development part of a frame start (plan "The frame in Debug/Development builds", step 4): engine controls,
/// developer requests and chords, the panel's input, the DevPanel blocker; while paused, paused ticks until a step or a
/// resume (the frame clock restarted and audio released after them). @return false when the game must quit (the
/// platform closed or the PC asked to quit during a paused tick).
bool dev_frame_begin(std::uint64_t next_frame, platform::InputSample& sample, std::uint32_t dev_requests);

/// live::pause(): pause at the start of the next frame.
void dev_request_pause();

/// Forgets the pause (nnh::init()).
void dev_pause_reset();

/// Scene changes were applied at the start of the frame that paused: the held frame can't be replayed.
extern bool g_dev_scenes_changed;

/// The platform's developer requests of this frame start (polled once, shared with the overlay).
extern std::uint32_t g_dev_frame_requests;

} // namespace nnh::detail
