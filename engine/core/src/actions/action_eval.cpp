// One evaluation of a set (spec 017 FR-11..FR-15; plan 017 "Semantics"): contexts and their enable edges,
// per-slot blocking (FR-13), slots combined (FR-7), edges, counters and repeat. No allocation, no logging in the
// normal path.

#include "actions/actions_internal.hpp"
#include "nnh/build.hpp"
#include "nnh/profile.hpp"

namespace nnh::detail {
namespace {

constexpr std::uint8_t kEdges = action_flag::kPressed | action_flag::kReleased | action_flag::kRepeated;
constexpr std::uint32_t kNever = 0xFFFFFFFFu;

std::uint32_t saturating_next(std::uint32_t v) { return v == kNever ? v : v + 1; }
float magnitude(float v) { return v < 0.0f ? -v : v; }

/// Idle without edges or events (disabled context, listening). Counters restart when `restart`.
void go_idle(ActionRec& r, bool restart) {
    r.flags = static_cast<std::uint8_t>(r.flags & ~(kEdges | action_flag::kHeld));
    r.out = {};
    r.value = 0;
    r.held_frames = 0;
    r.since_pressed = restart ? kNever : saturating_next(r.since_pressed);
    for (SlotRec& s : r.slots) s.state = static_cast<std::uint8_t>(s.state & ~slot_state::kHeld);
}

bool usable(const ActionRec& r) {
    for (const SlotRec& s : r.slots) {
        if ((s.flags & slot_flag::kActive) != 0) return true;
    }
    return false;
}

/// FR-9: in Development, the first time a context is enabled with an action no slot of which works here.
void warn_unusable(ActionBlock& b, int context) {
    if constexpr (build::kDevToolsEnabled) {
        ContextRec& ctx = b.ctx[context];
        if ((ctx.flags & context_flag::kWarned) != 0) return;
        for (int i = 0; i < b.declared; ++i) {
            const ActionRec& r = b.actions[b.order[i]];
            if (r.context != context || usable(r)) continue;
            ctx.flags = static_cast<std::uint8_t>(ctx.flags | context_flag::kWarned);
            NNH_LOG_WARN(actions, "context \"%s\" enabled with action \"%s\" unusable on this system (no active "
                                  "binding: add a fallback)", ctx.name, r.name);
            return;
        }
    }
}

/// FR-13: the slots of the context's actions whose control is held right now are blocked until released.
void block_held(ActionBlock& b, int context, const InputView& v) {
    for (int i = 0; i < b.declared; ++i) {
        ActionRec& r = b.actions[b.order[i]];
        if (r.context != context) continue;
        for (SlotRec& s : r.slots) {
            if (slot_engaged(s, v, s.press / 100.0f)) s.state = static_cast<std::uint8_t>(s.state | slot_state::kBlocked);
        }
    }
}

SlotOut read_slots(ActionRec& r, const InputView& v) {
    SlotOut acc;
    for (SlotRec& s : r.slots) {
        if ((s.flags & slot_flag::kActive) == 0) continue;
        if ((s.state & slot_state::kBlocked) != 0) {
            if (slot_engaged(s, v, s.release / 100.0f)) continue;
            s.state = static_cast<std::uint8_t>(s.state & ~slot_state::kBlocked);
        }
        combine(r.kind, acc, eval_slot(r.kind, s, v));
    }
    return acc;
}

bool repeat_now(const ActionRec& r) {
    if (r.repeat.interval == 0 || r.held_frames == 0) return false;
    const std::uint32_t after = r.held_frames - 1;
    return after >= r.repeat.delay && (after - r.repeat.delay) % r.repeat.interval == 0;
}

void evaluate_action(ActionRec& r, const InputView& v) {
    const SlotOut acc = read_slots(r, v);
    const bool was = (r.flags & action_flag::kHeld) != 0;
    const bool held = r.kind == ActionKind::Button
                          ? acc.held
                          : held_with_hysteresis(magnitude(acc.value), was, r.press / 100.0f, r.release / 100.0f);
    std::uint8_t flags = static_cast<std::uint8_t>(r.flags & ~(kEdges | action_flag::kHeld));
    if (held) flags |= action_flag::kHeld;
    if (held && !was) flags |= action_flag::kPressed;
    if (!held && was) flags |= action_flag::kReleased;
    r.held_frames = held ? saturating_next(r.held_frames) : 0;
    r.since_pressed = (flags & action_flag::kPressed) != 0 ? 0 : saturating_next(r.since_pressed);
    r.out = acc.vec;
    r.value = acc.value;
    if ((flags & action_flag::kPressed) != 0 || repeat_now(r)) flags |= action_flag::kRepeated;
    r.flags = flags;
}

} // namespace

void evaluate(ActionBlock& b, const InputView& v) {
    NNH_PROFILE_SCOPE("actions");
    if (b.listen.active) {
        listen_step(b, v);
        for (int i = 0; i < b.declared; ++i) go_idle(b.actions[b.order[i]], false);
        for (ContextRec& ctx : b.ctx) ctx.flags = static_cast<std::uint8_t>(ctx.flags & ~context_flag::kOn);
        return;
    }
    bool on[kMaxContexts] = {};
    for (int c = 0; c < b.contexts; ++c) {
        ContextRec& ctx = b.ctx[c];
        on[c] = (ctx.flags & context_flag::kDeclared) != 0 && context_on(b, c);
        const bool was = (ctx.flags & context_flag::kOn) != 0;
        ctx.flags = static_cast<std::uint8_t>(on[c] ? ctx.flags | context_flag::kOn : ctx.flags & ~context_flag::kOn);
        if (on[c] && !was) {
            block_held(b, c, v);
            warn_unusable(b, c);
        }
    }
    for (int i = 0; i < b.declared; ++i) {
        ActionRec& r = b.actions[b.order[i]];
        if (on[r.context]) {
            evaluate_action(r, v);
        } else {
            go_idle(r, true);
        }
    }
}

} // namespace nnh::detail
