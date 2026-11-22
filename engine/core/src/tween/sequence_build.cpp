// Building sequences (spec 015 FR-10): a sequence slot on the start-order list, its steps appended in place (one
// step record + one child slot each, no allocation), limits, and a failed build that removes the whole sequence.

#include "nnh/assert.hpp"
#include "tween/tween_internal.hpp"

#include <utility>

namespace nnh {
namespace detail {
namespace {

int take_step(TweenBlock& b) {
    const int st = b.step_free_head;
    if (st >= 0) b.step_free_head = b.steps[st].next;
    return st;
}

/// The last step of a sequence (or -1), its step count, and whether every step so far is in the first group.
struct Tail {
    int last = -1;
    int count = 0;
    bool first_group = true;
};

Tail tail_of(const TweenBlock& b, const SlotHot& s) {
    Tail t;
    for (int st = s.first_step; st >= 0; st = b.steps[st].next) {
        if (t.count > 0 && (b.steps[st].flags & step_flag::kJoin) == 0) t.first_group = false;
        t.last = st;
        ++t.count;
    }
    return t;
}

const char* check_step(const TweenBlock& b, const SlotHot& s, const Tail& t, const TweenOptions& o) {
    if ((s.flags & slot_flag::kStarted) != 0) return "steps appended after the sequence started";
    if (t.count >= kMaxSequenceSteps) return "more than kMaxSequenceSteps (64) steps in a sequence";
    if (o.loops == kForever) return "a sequence step can't loop forever";
    if (b.step_free_head < 0) return "out of sequence steps: raise TweenSetConfig::steps";
    return nullptr;
}

} // namespace
} // namespace detail

SequenceBuilder TweenSet::sequence(const SequenceOptions& o) {
    using namespace detail;
    if (!o.delay.valid() || (o.loops < 1 && o.loops != kForever)) {
        tween_reject("a sequence needs a delay within 0..1 hour and loops ≥ 1 or nnh::kForever");
        return {block_, {}};
    }
    const int index = tween_take_slot(block_);
    if (index < 0) return {block_, {}};
    const TweenOptions options{.delay = o.delay, .loops = o.loops, .loop_mode = o.loop_mode,
                               .on_finish = o.on_finish, .on_loop = o.on_loop, .publish = o.publish};
    TweenSpec spec;
    spec.kind = TweenKind::Sequence;
    SlotHot& s = block_->hot[index];
    tween_fill(s, block_->cold[index], spec, FrameTime{}, options);
    tween_append(*block_, index);
    return {block_, {static_cast<std::uint16_t>(index), block_->serial, s.generation}};
}

SequenceBuilder& SequenceBuilder::append(const detail::TweenSpec& spec, FrameTime time, const TweenOptions& options,
                                         bool call) {
    using namespace detail;
    const bool join = std::exchange(join_next_, false);
    const int seq = tween_find(block_, handle_);
    if (seq < 0) return *this; // a failed or ended sequence: ignored (the failure was reported)
    SlotHot& s = block_->hot[seq];
    const Tail tail = tail_of(*block_, s);
    const char* problem = check_step(*block_, s, tail, options);
    int child = -1;
    if (problem == nullptr && !tween_valid_request(spec, time, options)) problem = "an invalid step";
    if (problem == nullptr && (child = tween_take_slot(block_)) < 0) problem = "the set is full";
    if (problem != nullptr) {
        NNH_LOG_ERROR(tween, "TweenSet: sequence not started: %s", problem);
        NNH_ASSERT(false, "nnh::SequenceBuilder: %s", problem);
        tween_kill_slot(*block_, seq); // frees the partial sequence (or marks it, inside an update)
        handle_ = {};
        return *this;
    }
    SlotHot& c = block_->hot[child];
    tween_fill(c, block_->cold[child], spec, time, options);
    const int st = take_step(*block_);
    block_->steps[st] = Step{static_cast<std::int16_t>(child), -1,
                             static_cast<std::uint8_t>((join && tail.count > 0 ? step_flag::kJoin : 0) |
                                                       (call ? step_flag::kCall : 0)),
                             {}};
    if (tail.last >= 0) block_->steps[tail.last].next = static_cast<std::int16_t>(st);
    else s.first_step = static_cast<std::int16_t>(st);
    StepPlace places[kMaxSequenceSteps];
    std::int64_t total = 0;
    sequence_layout(*block_, seq, places, total);
    if (total > FrameTime::kMaxTicks) {
        NNH_LOG_ERROR(tween, "TweenSet: sequence not started: longer than 1 hour");
        NNH_ASSERT(false, "nnh::SequenceBuilder: a sequence longer than 1 hour");
        tween_kill_slot(*block_, seq);
        handle_ = {};
        return *this;
    }
    s.duration = static_cast<std::int32_t>(total);
    const bool first_group = tail.count == 0 || (join && tail.first_group);
    if (first_group && s.delay == 0 && c.delay == 0 && (c.flags & slot_flag::kHasFrom) != 0) {
        c.flags |= slot_flag::kStarted; // FR-6: the first drawn frame already shows `from`
        tween_write(c, c.from);
    }
    return *this;
}

SequenceBuilder& SequenceBuilder::join() {
    join_next_ = true;
    return *this;
}

SequenceBuilder& SequenceBuilder::wait(FrameTime time) { return append({}, time, {}); }

SequenceBuilder& SequenceBuilder::call(TweenCallback fn) { return append({}, Frames{0}, {.on_finish = fn}, true); }

} // namespace nnh
