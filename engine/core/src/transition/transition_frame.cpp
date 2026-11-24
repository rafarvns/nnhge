// One frame of the running transition (plan 023 "One frame of the hook"), run by scenes::update_and_draw() through
// the scene stack's hook: step the timeline (covered frames: on_covered, the loading step), update the top scene only
// when the options allow it, draw every scene, draw the overlay, then the end of the frame (enqueue the change, the
// next phase, or done). Done: the block lifted, the hook cleared, the state idle, then on_finished, then the game's
// finish event. Cancel (shutdown, the end of run()): silent.

#include "input/input_block.hpp"
#include "nnh/app.hpp"
#include "nnh/events.hpp"
#include "nnh/profile.hpp"
#include "nnh/scene.hpp"
#include "scene/scene_hook.hpp"
#include "transition/transition_internal.hpp"

namespace nnh::detail {
namespace {

using transition::Phase;

constexpr SceneTransitionHook kHook{&transition_frame, &transition_cancel};

/// Ends the transition: nothing of it is left running (the hook, the block, the waiting scene's count).
void stop(TransitionState& t) {
    if (t.holds_target) --scene_pending_count(*t.target);
    if (t.blocking) input_block(InputBlocker::Transition, false);
    t.holds_target = false;
    t.blocking = false;
    t.running = false;
    g_scene_transition = nullptr;
}

void finish(TransitionState& t) {
    const transition::Callback on_finished = t.on_finished; // copies: on_finished may start the next transition
    const FinishEvent publish = t.publish;
    stop(t);
    if (on_finished) on_finished();
    if (publish) events_publish(publish.type, publish.bytes, publish.size);
}

void step(TransitionState& t) {
    ++t.frames;
    // With no out phase, a change enqueued during this very frame (a request before update_and_draw()) is applied at
    // the start of the next one: the covered phase starts there, so on_covered always follows the change.
    if (t.change_waiting && static_cast<std::uint32_t>(frame_count()) == t.request_frame) return;
    t.change_waiting = false;
    timeline_step(t.timeline, t.out_ease, t.in_ease);
    if (t.timeline.phase == Phase::Covered) transition_covered_frame(t);
}

void end_of_frame(TransitionState& t) {
    const bool skip_in = t.op != TransitionOp::Cover && scenes::size() == 0; // an in phase over nothing (FR-19)
    switch (timeline_end(t.timeline, t.loading_done, skip_in)) {
    case TimelineEnd::OutEnded: transition_enqueue_change(t); break;
    case TimelineEnd::Done: finish(t); break;
    case TimelineEnd::Continue: break;
    }
}

} // namespace

void transition_install_hook() { g_scene_transition = &kHook; }

void transition_frame() {
    TransitionState& t = g_transition;
    {
        NNH_PROFILE_SCOPE("transition");
        step(t);
    }
    const Phase phase = t.timeline.phase;
    if ((phase == Phase::Out && t.update_out) || (phase == Phase::In && t.update_in)) scenes_update_top();
    scenes_draw_all();
    NNH_PROFILE_SCOPE("transition");
    transition_draw_overlay(t);
    end_of_frame(t);
}

void transition_cancel() {
    TransitionState& t = g_transition;
    if (t.running) stop(t);
    g_scene_transition = nullptr;
}

void transition_enqueue_change(TransitionState& t) {
    if (t.holds_target) {
        --scene_pending_count(*t.target); // the stack's own request takes it over
        t.holds_target = false;
    }
    switch (t.op) {
    case TransitionOp::Push: scenes_request_change(SceneChange::Push, t.target); break;
    case TransitionOp::Replace: scenes_request_change(SceneChange::Replace, t.target); break;
    case TransitionOp::Pop:
        if (scenes::size() == 0) {
            NNH_LOG_ERROR(transition, "transition pop: nothing to pop (the game emptied the stack)");
        }
        scenes_request_change(SceneChange::TransitionPop, nullptr);
        break;
    case TransitionOp::Cover: return;
    }
    t.request_frame = static_cast<std::uint32_t>(frame_count());
    t.change_waiting = true;
}

} // namespace nnh::detail
