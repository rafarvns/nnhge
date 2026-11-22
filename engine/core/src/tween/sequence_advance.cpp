// Playing sequences (spec 015 FR-10, FR-16): each update moves the sequence's position along its loop (forwards,
// or backwards in a yoyo loop) and plays every step that the move crosses. A step is evaluated at its local time,
// so leftover time carries across steps and loops, a yoyo loop mirrors every step, and nothing drifts.
//
// Positions are integer ticks within the loop. A forward move (p0, p1] crosses what lies after p0 up to p1 (p0 is
// -1 at the start of the first loop and of every restart, so steps at 0 play); a backward move [p1, p0) mirrors
// it, so a call step at a turning point never runs twice in a row.

#include "nnh/assert.hpp"
#include "tween/tween_internal.hpp"

namespace nnh::detail {
namespace {

using std::int64_t;

struct Play {
    TweenBlock& b;
    int seq;
    std::uint32_t generation;
    bool alive() const { return b.hot[seq].generation == generation; } // a callback may end the sequence
};

/// Writes child `c` at local time `t` (its delay, loops and yoyo included).
void write_child_at(SlotHot& c, int64_t t, int64_t length) {
    const bool yoyo = (c.flags & slot_flag::kYoyo) != 0;
    if (t >= length) {
        tween_write(c, yoyo && c.loops_left % 2 == 0 ? c.from : c.to);
        return;
    }
    if (t <= c.delay) {
        tween_write(c, c.from);
        return;
    }
    const int64_t u = t - c.delay; // here duration > 0, since delay < t < delay + duration × loops
    const int64_t loop = u / c.duration;
    c.elapsed = static_cast<std::int32_t>(u - loop * c.duration);
    if (yoyo && (loop & 1) != 0) {
        c.flags |= slot_flag::kBackwards;
    } else {
        c.flags &= static_cast<std::uint8_t>(~slot_flag::kBackwards);
    }
    tween_write_current(c);
}

bool child_loops(Play& p, int child, int64_t t0, int64_t t1) {
    const SlotHot& c = p.b.hot[child];
    if (c.duration <= 0 || c.loops_left <= 1 || t1 < c.delay) return true;
    const int64_t first = t0 < c.delay ? 1 : (t0 - c.delay) / c.duration + 1;
    int64_t last = (t1 - c.delay) / c.duration;
    if (last > c.loops_left - 1) last = c.loops_left - 1;
    for (int64_t k = first; k <= last; ++k) {
        tween_loop_callback(p.b, child);
        if (!p.alive()) return false;
    }
    return true;
}

bool play_forward(Play& p, const StepPlace* places, int n, int64_t p0, int64_t p1) {
    for (int i = 0; i < n; ++i) {
        const Step& step = p.b.steps[places[i].step];
        SlotHot& c = p.b.hot[step.child];
        const int64_t t0 = p0 - places[i].start, t1 = p1 - places[i].start, length = places[i].length;
        if ((step.flags & step_flag::kCall) != 0) {
            if (t0 < 0 && t1 >= 0) tween_run_completion(p.b, step.child);
            if (!p.alive()) return false;
            continue;
        }
        if (t1 < c.delay || t0 >= length) continue; // not started yet, or done before this move
        if ((c.flags & slot_flag::kStarted) == 0) tween_begin(c); // captured when the sequence reaches it
        write_child_at(c, t1 < length ? t1 : length, length);
        if (!child_loops(p, step.child, t0, t1)) return false;
        if (t1 >= length) tween_run_completion(p.b, step.child);
        if (!p.alive()) return false;
    }
    return true;
}

bool play_backward(Play& p, const StepPlace* places, int n, int64_t p0, int64_t p1) {
    for (int i = n - 1; i >= 0; --i) {
        const Step& step = p.b.steps[places[i].step];
        SlotHot& c = p.b.hot[step.child];
        const int64_t t0 = p0 - places[i].start, t1 = p1 - places[i].start, length = places[i].length;
        if ((step.flags & step_flag::kCall) != 0) {
            if (t1 <= 0 && t0 > 0) tween_run_completion(p.b, step.child);
            if (!p.alive()) return false;
            continue;
        }
        if (t0 <= c.delay || t1 >= length) continue; // already at its start, or still after its end
        if ((c.flags & slot_flag::kStarted) == 0) tween_begin(c);
        write_child_at(c, t1, length);
    }
    return true;
}

bool reject_endless(TweenBlock& b, int index) {
    if (b.hot[index].duration != 0 || b.hot[index].loops_left != kForever) return false;
    NNH_LOG_ERROR(tween, "TweenSet: an infinite sequence of length 0 (removed)");
    NNH_ASSERT(false, "nnh::TweenSet: an infinite sequence of length 0");
    tween_kill_slot(b, index);
    return true;
}

} // namespace

void sequence_advance(TweenBlock& b, int index, std::int32_t ticks) {
    SlotHot& s = b.hot[index];
    if ((s.flags & slot_flag::kStarted) == 0) {
        if (ticks < s.delay) {
            s.delay -= ticks;
            return;
        }
        ticks -= s.delay;
        s.delay = 0;
        s.flags |= slot_flag::kStarted | slot_flag::kFresh;
    }
    if (reject_endless(b, index)) return;
    StepPlace places[kMaxSequenceSteps];
    int64_t total = 0;
    const int n = sequence_layout(b, index, places, total);
    Play p{b, index, s.generation};
    const std::int32_t length = s.duration;
    for (;;) {
        const std::int32_t e0 = s.elapsed;
        const std::int32_t e1 = ticks < length - e0 ? e0 + ticks : length;
        ticks -= e1 - e0;
        const bool backwards = (s.flags & slot_flag::kBackwards) != 0;
        const bool fresh = (s.flags & slot_flag::kFresh) != 0;
        s.flags &= static_cast<std::uint8_t>(~slot_flag::kFresh);
        s.elapsed = e1;
        const bool going = backwards ? play_backward(p, places, n, length - e0, length - e1)
                                     : play_forward(p, places, n, fresh ? -1 : e0, e1);
        if (!going || (s.flags & slot_flag::kPaused) != 0 || e1 < length) return;
        if (s.loops_left != kForever && --s.loops_left == 0) {
            tween_finish(b, index, backwards);
            return;
        }
        s.elapsed = 0;
        if ((s.flags & slot_flag::kYoyo) != 0) {
            s.flags ^= slot_flag::kBackwards; // turn around: the turning point is not played again
        } else {
            s.flags |= slot_flag::kFresh; // restart: steps at 0 play again
        }
        tween_loop_callback(b, index);
        if (!p.alive() || (s.flags & slot_flag::kPaused) != 0) return;
    }
}

void sequence_jump_to_end(TweenBlock& b, int index, bool backwards) {
    StepPlace places[kMaxSequenceSteps];
    int64_t total = 0;
    const int n = sequence_layout(b, index, places, total);
    for (int k = 0; k < n; ++k) {
        const int i = backwards ? n - 1 - k : k;
        SlotHot& c = b.hot[b.steps[places[i].step].child];
        if ((c.flags & slot_flag::kStarted) == 0) tween_begin(c);
        if (backwards) {
            tween_write(c, c.from);
        } else {
            write_child_at(c, places[i].length, places[i].length);
        }
    }
}

} // namespace nnh::detail
