// The four requests and reveal_at (spec 023 FR-1, FR-19, FR-22; plan 023 "Requests"): refused while another
// transition runs; misuse asserts in Debug/Development and is clamped or refused with an error log in Release; the
// options resolved into the state; the waiting scene's pending count held; the hook installed, input blocked and,
// with no out phase, the change enqueued at once. No allocation.

#include "input/input_block.hpp"
#include "nnh/app.hpp"
#include "nnh/assert.hpp"
#include "nnh/scene.hpp"
#include "scene/scene_hook.hpp"
#include "transition/transition_internal.hpp"

namespace nnh {
namespace {

using detail::TransitionOp;
using detail::TransitionState;
using transition::Effect;
using transition::Kind;
using transition::Options;

/// Release: logged and handled; Debug/Development: stops at the assert.
#define NNH_TRANSITION_MISUSE(...)                                                                               \
    do {                                                                                                         \
        NNH_ASSERT(false, __VA_ARGS__);                                                                          \
        NNH_LOG_ERROR(transition, __VA_ARGS__);                                                                  \
    } while (0)

int checked_frames(FrameTime time, const char* name) {
    if (!time.valid()) {
        NNH_TRANSITION_MISUSE("transition %s time is invalid (negative or over 1 hour): 0 frames used", name);
        return 0;
    }
    const int frames = (time.ticks + FrameTime::kTicksPerFrame / 2) / FrameTime::kTicksPerFrame;
    if (frames > detail::kMaxPhaseFrames) {
        NNH_TRANSITION_MISUSE("transition %s time over 10 s: 600 frames used", name);
    }
    return detail::frames_of(time);
}

bool finite(float v) { return v >= -3.4e38f && v <= 3.4e38f; }

/// Checks one effect's settings; returns it with the misuse fixed (Release).
Effect checked(Effect e, bool custom_given, bool out) {
    if (out && e.kind == Kind::Same) {
        NNH_TRANSITION_MISUSE("transition effect is same(): give an effect (fade() used)");
        e = transition::fade(e.color);
    }
    if (e.kind == Kind::Custom && !custom_given) {
        NNH_TRANSITION_MISUSE("transition custom() effect without Options::custom (fade() used)");
        e = transition::fade(e.color);
    }
    if (e.kind == Kind::Blinds && (e.bars < 2 || e.bars > 32)) {
        NNH_TRANSITION_MISUSE("transition blinds bars %d outside 2..32 (clamped)", e.bars);
    }
    if (e.kind == Kind::Iris && (e.band < 1 || e.band > 16)) {
        NNH_TRANSITION_MISUSE("transition iris band %d outside 1..16 (clamped)", e.band);
    }
    if (e.kind == Kind::Iris && e.center.given && (!finite(e.center.point.x) || !finite(e.center.point.y))) {
        NNH_TRANSITION_MISUSE("transition iris centre is NaN or infinite (the middle used)");
        e.center = {};
    }
    return e;
}

void resolve(TransitionState& t, const Options& o) {
    const bool has_custom = static_cast<bool>(o.custom);
    Options fixed = o;
    fixed.effect = checked(o.effect, has_custom, true);
    fixed.reveal = checked(o.reveal, has_custom, false);
    fixed.bottom = checked(o.bottom, has_custom, false);
    fixed.bottom_reveal = checked(o.bottom_reveal, has_custom, false);
    if (o.tall && (o.bottom.kind != Kind::Same || o.bottom_reveal.kind != Kind::Same)) {
        NNH_TRANSITION_MISUSE("transition tall: bottom and bottom_reveal must be same() (ignored)");
    }
    if (o.tall_gap < 0 || o.tall_gap > 240) {
        NNH_TRANSITION_MISUSE("transition tall_gap %d outside 0..240 (clamped)", o.tall_gap);
    }
    const int gap = o.tall_gap < 0 ? 0 : (o.tall_gap > 240 ? 240 : o.tall_gap);
    const detail::EffectArea area{o.tall, gap};
    const detail::ScreenEffects s = detail::resolve_screens(fixed);
    const bool cont = o.reveal_style == transition::Reveal::Continue;
    t.fx[detail::kTopOut] = detail::resolve_effect(s.top_out, Screen::Top, area, false);
    t.fx[detail::kTopIn] = detail::resolve_effect(s.top_in, Screen::Top, area, cont);
    t.fx[detail::kBottomOut] = detail::resolve_effect(s.bottom_out, Screen::Bottom, area, false);
    t.fx[detail::kBottomIn] = detail::resolve_effect(s.bottom_in, Screen::Bottom, area, cont);
    t.tall = o.tall;
    t.gap = static_cast<std::uint8_t>(gap);
}

bool refused_while_running(TransitionState& t) {
    if (!t.running) return false;
    if (!t.refusal_logged) {
        t.refusal_logged = true;
        NNH_LOG_DEBUG(transition, "request refused: a transition is running");
    }
    return true;
}

bool start(TransitionOp op, Scene* scene, const Options& o) {
    TransitionState& t = detail::g_transition;
    if (refused_while_running(t)) return false;
    if (scene != nullptr && scene->on_stack()) {
        NNH_TRANSITION_MISUSE("transition push/replace of a scene already on the stack (refused)");
        return false;
    }
    if (op == TransitionOp::Pop && scenes::size() == 0) {
        NNH_TRANSITION_MISUSE("transition pop with an empty scene stack (refused)");
        return false;
    }
    const int out = checked_frames(o.out, "out");
    const int hold = checked_frames(o.hold, "hold");
    const int in = checked_frames(o.in, "in");
    t = TransitionState{};
    resolve(t, o);
    t.custom = o.custom;
    t.loading = o.loading;
    t.on_covered = o.on_covered;
    t.on_finished = o.on_finished;
    t.publish = o.publish;
    t.out_ease = o.out_ease;
    t.in_ease = o.in_ease;
    t.update_out = o.update_out;
    t.update_in = o.update_in;
    t.op = op;
    t.loading_progress = o.loading ? 0.0f : 1.0f;
    detail::timeline_start(t.timeline, out, hold, in);
    if (scene != nullptr) {
        t.target = scene;
        t.holds_target = true;
        ++detail::scene_pending_count(*scene);
    }
    t.running = true;
    detail::transition_install_hook();
    if (o.block_input) {
        t.blocking = true;
        detail::input_block(detail::InputBlocker::Transition, true);
    }
    if (out == 0) detail::transition_enqueue_change(t);
    return true;
}

} // namespace

bool transition::replace(Scene& scene, const Options& options) {
    return start(TransitionOp::Replace, &scene, options);
}
bool transition::push(Scene& scene, const Options& options) { return start(TransitionOp::Push, &scene, options); }
bool transition::pop(const Options& options) { return start(TransitionOp::Pop, nullptr, options); }
bool transition::cover(const Options& options) { return start(TransitionOp::Cover, nullptr, options); }

bool transition::reveal_at(IrisCenter center) {
    TransitionState& t = detail::g_transition;
    if (!t.running || t.timeline.phase == Phase::In) return false;
    if (center.given && (!finite(center.point.x) || !finite(center.point.y))) {
        NNH_TRANSITION_MISUSE("transition reveal_at: the centre is NaN or infinite (the middle used)");
        center = {};
    }
    const detail::EffectArea area{t.tall, t.gap};
    for (int i : {detail::kTopIn, detail::kBottomIn}) {
        Effect e;
        e.kind = Kind::Iris;
        e.center = center;
        const Screen screen = i == detail::kTopIn ? Screen::Top : Screen::Bottom;
        const detail::ResolvedEffect placed = detail::resolve_effect(e, screen, area, false);
        t.fx[i].cx = placed.cx;
        t.fx[i].cy = placed.cy;
    }
    return true;
}

} // namespace nnh
