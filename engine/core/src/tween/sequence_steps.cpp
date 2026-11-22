// The step list of a sequence (spec 015 FR-10): layout in groups, ending and freeing the children with their
// sequence, and kill_target's search. Children are slots off the start-order list, reached only through steps.

#include "tween/tween_internal.hpp"

namespace nnh::detail {

int sequence_layout(const TweenBlock& b, int index, StepPlace out[kMaxSequenceSteps], std::int64_t& total) {
    int count = 0;
    std::int64_t group_start = 0, group_length = 0;
    for (int st = b.hot[index].first_step; st >= 0 && count < kMaxSequenceSteps; st = b.steps[st].next) {
        const SlotHot& c = b.hot[b.steps[st].child];
        if (count > 0 && (b.steps[st].flags & step_flag::kJoin) == 0) { // a new group starts after the last one
            group_start += group_length;
            group_length = 0;
        }
        const std::int64_t length = c.delay + static_cast<std::int64_t>(c.duration) * c.loops_left;
        if (length > group_length) group_length = length;
        out[count++] = {static_cast<std::int16_t>(st), static_cast<std::int32_t>(group_start),
                        static_cast<std::int32_t>(length)};
    }
    total = group_start + group_length;
    return count;
}

void sequence_kill_children(TweenBlock& b, int index) {
    for (int st = b.hot[index].first_step; st >= 0; st = b.steps[st].next) {
        SlotHot& c = b.hot[b.steps[st].child];
        if (++c.generation == 0) c.generation = 1;
        c.state = SlotState::Dead;
    }
}

void sequence_release(TweenBlock& b, int index) {
    SlotHot& s = b.hot[index];
    for (int st = s.first_step; st >= 0;) {
        Step& step = b.steps[st];
        const int next = step.next;
        SlotHot& c = b.hot[step.child];
        c.state = SlotState::Free;
        c.prev = -1;
        c.next = b.free_head;
        b.free_head = step.child;
        step.child = -1;
        step.next = b.step_free_head;
        b.step_free_head = static_cast<std::int16_t>(st);
        st = next;
    }
    s.first_step = -1;
}

bool sequence_has_target(const TweenBlock& b, int index, const void* target) {
    for (int st = b.hot[index].first_step; st >= 0; st = b.steps[st].next) {
        if (b.hot[b.steps[st].child].target == target) return true;
    }
    return false;
}

} // namespace nnh::detail
