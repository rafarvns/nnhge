#pragma once
// The running transition (spec 023, decision D38): one per engine, module state reached only through
// nnh::transition functions, run by the scene stack's hook (scene/scene_hook.hpp) while it runs. The frame timeline is
// pure (transition_timeline.cpp, tests/transition/test_timeline.cpp). Internal.

#include <cstdint>

#include "nnh/detail/target_layout.hpp"
#include "nnh/log.hpp"
#include "nnh/transition.hpp"
#include "transition/effect_geometry.hpp"

NNH_LOG_CHANNEL_EXTERN(transition);

namespace nnh::detail {

/// Longest phase: 10 seconds of 60 Hz frames (FR-3).
inline constexpr int kMaxPhaseFrames = 600;

/// The frame timeline (plan 023 "The frame timeline"): whole frames per phase, coverage per frame.
struct Timeline {
    std::uint16_t out = 0;  ///< O, H, I in frames (0..600)
    std::uint16_t hold = 0;
    std::uint16_t in = 0;
    std::uint16_t k = 0;        ///< frame of the out or in phase (1-based once stepped)
    std::uint32_t covered = 0;  ///< covered frames so far
    transition::Phase phase = transition::Phase::None;
    float coverage = 0;         ///< this frame's, ratcheted (monotonic within a phase)
};

/// What the end of a frame decided.
enum class TimelineEnd : std::uint8_t { Continue, OutEnded, Done };

/// Whole frames of a duration: ⌊(ticks + 2048) / 4096⌋, clamped to 0..600 (an invalid time counts 0).
int frames_of(FrameTime time);
/// Clamps to 0..1 (NaN → 0).
float clamp_unit(float value);
/// Starts at frame 0 of the out phase (or of the covered phase when out is 0: coverage 1 from the start).
void timeline_start(Timeline& t, int out, int hold, int in);
/// Steps one frame: k or the covered count advances and the coverage is computed (out: ratchet up of
/// ease(k / O), 1 at k = O; covered: 1; in: ratchet down of ease((I − k) / I), 0 at k = I).
void timeline_step(Timeline& t, const Easing& out_ease, const Easing& in_ease);
/// The end of a frame: out complete → covered (OutEnded); covered complete (loading done and at least max(H, 1)
/// covered frames) → in, or Done when I = 0 or `skip_in`; in complete → Done.
TimelineEnd timeline_end(Timeline& t, bool loading_done, bool skip_in);
/// out/in: k / n; covered: the hold's progress (1 without a hold).
float timeline_phase_progress(const Timeline& t);

/// The stack change a transition makes.
enum class TransitionOp : std::uint8_t { Cover, Push, Replace, Pop };

/// Indexes of TransitionState::fx.
inline constexpr int kTopOut = 0, kTopIn = 1, kBottomOut = 2, kBottomIn = 3;

struct TransitionState {
    transition::CustomEffect custom{};
    transition::LoadingStep loading{};
    transition::Callback on_covered{};
    transition::Callback on_finished{};
    FinishEvent publish{};
    ResolvedEffect fx[4]{}; ///< kTopOut, kTopIn, kBottomOut, kBottomIn
    Easing out_ease{};
    Easing in_ease{};
    Scene* target = nullptr; ///< push/replace: the scene waiting (its pending count held until enqueued)
    Timeline timeline{};
    std::uint32_t frames = 0;        ///< transition frames so far
    std::uint32_t request_frame = 0; ///< nnh::frame_count() when the change was enqueued
    float loading_progress = 0;
    TransitionOp op = TransitionOp::Cover;
    std::uint8_t gap = 0; ///< tall area gap, 0..240
    bool running : 1 = false;
    bool tall : 1 = false;
    bool update_out : 1 = false;
    bool update_in : 1 = false;
    bool blocking : 1 = false;     ///< the transition's input blocker is on
    bool holds_target : 1 = false; ///< `target`'s pending count is held by the transition
    bool change_waiting : 1 = false; ///< the change was enqueued this frame: covered frames wait for it
    bool loading_done : 1 = false;
    bool refusal_logged : 1 = false;
    bool warned : 1 = false;
};
static_assert(!detail::kThreeDsLayout || sizeof(TransitionState) <= 256,
              "transition state ≤ 256 bytes on the 3DS (FR-21)");
static_assert(sizeof(void*) != 8 || sizeof(TransitionState) <= 320, "transition state ≤ 320 bytes on 64-bit PCs");

/// The one transition (transition_queries.cpp).
extern TransitionState g_transition;

/// The hook's frame and cancel (transition_frame.cpp), and the change made at the end of the out phase.
void transition_frame();
void transition_cancel();
void transition_enqueue_change(TransitionState& t);
/// The scene-stack hook pointing at transition_frame/transition_cancel (transition_frame.cpp).
void transition_install_hook();
/// A covered frame's work: on_covered on the first, then the loading step (transition_loading.cpp).
void transition_covered_frame(TransitionState& t);
/// The overlay of this frame: each screen's effect for the phase (transition_overlay.cpp).
void transition_draw_overlay(const TransitionState& t);

} // namespace nnh::detail
