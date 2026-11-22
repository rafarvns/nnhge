// Starting tweens and timers (spec 015 FR-4..FR-6, FR-13): checks, a free slot appended to the start-order
// list, `from` written at creation when there is no delay.

#include "nnh/assert.hpp"
#include "tween/tween_internal.hpp"

namespace nnh {
namespace detail {
namespace {

bool reject(const char* why) {
    NNH_LOG_ERROR(tween, "TweenSet: %s (not started)", why);
    NNH_ASSERT(false, "nnh::TweenSet: %s", why);
    return false;
}

bool valid_request(const TweenSpec& spec, FrameTime time, const TweenOptions& o) {
    if (!time.valid()) return reject("the time is negative or over 1 hour");
    if (!o.delay.valid()) return reject("the delay is negative or over 1 hour");
    if (o.loops < 1 && o.loops != kForever) return reject("loops must be at least 1 or nnh::kForever");
    if (o.loops == kForever && time.ticks == 0) return reject("an infinite loop of length 0");
    if (spec.kind != TweenKind::Timer && spec.target == nullptr &&
        (spec.setter == nullptr || !*spec.setter)) {
        return reject("the target is null");
    }
    return true;
}

int take_free_slot(TweenBlock* b) {
    if (b == nullptr || b->free_head < 0) {
        const int capacity = b != nullptr ? static_cast<int>(b->capacity) : 0;
        NNH_LOG_ERROR(tween, "TweenSet full (capacity %d): raise TweenSetConfig::tweens", capacity);
        NNH_ASSERT(false, "nnh::TweenSet full (capacity %d)", capacity);
        return -1;
    }
    const int index = b->free_head;
    b->free_head = b->hot[index].next;
    return index;
}

void append(TweenBlock& b, int index) {
    SlotHot& s = b.hot[index];
    s.prev = b.tail;
    s.next = -1;
    if (b.tail >= 0) {
        b.hot[b.tail].next = static_cast<std::int16_t>(index);
    } else {
        b.head = static_cast<std::int16_t>(index);
    }
    b.tail = static_cast<std::int16_t>(index);
    ++b.active;
}

std::uint8_t mode_flags(const TweenSpec& spec, const TweenOptions& o) {
    std::uint8_t flags = 0;
    if (o.loop_mode == LoopMode::Yoyo) flags |= slot_flag::kYoyo;
    if (spec.mode == TweenMode::By) flags |= slot_flag::kRelative;
    if (spec.mode == TweenMode::FromTo) flags |= slot_flag::kHasFrom;
    return flags;
}

void fill(SlotHot& s, SlotCold& c, const TweenSpec& spec, FrameTime time, const TweenOptions& o) {
    s.state = SlotState::Active;
    s.kind = spec.kind;
    s.flags = mode_flags(spec, o);
    s.ease = o.ease.builtin;
    s.custom = o.ease.custom;
    s.delay = o.delay.ticks;
    s.duration = time.ticks;
    s.elapsed = 0;
    s.loops_left = o.loops;
    s.target = spec.target;
    s.setter = spec.setter != nullptr && spec.target == nullptr ? *spec.setter : SetterFn{};
    for (float& v : s.from) {
        v = 0.0f;
    }
    for (float& v : s.to) {
        v = 0.0f;
    }
    if (spec.from != nullptr) tween_pack(spec.kind, spec.from, s.from);
    if (spec.end != nullptr) tween_pack(spec.kind, spec.end, s.to);
    c.on_finish = o.on_finish;
    c.on_loop = o.on_loop;
    c.publish = o.publish;
}

} // namespace
} // namespace detail

TweenHandle TweenSet::start(const detail::TweenSpec& spec, FrameTime time, const TweenOptions& options) {
    using namespace detail;
    if (!valid_request(spec, time, options)) return {};
    const int index = take_free_slot(block_);
    if (index < 0) return {};
    SlotHot& s = block_->hot[index];
    fill(s, block_->cold[index], spec, time, options);
    append(*block_, index);
    if ((s.flags & slot_flag::kHasFrom) != 0 && s.delay == 0) {
        s.flags |= slot_flag::kStarted; // nothing to capture: the first drawn frame already shows `from`
        tween_write(s, s.from);
    }
    return {static_cast<std::uint16_t>(index), block_->serial, s.generation};
}

TweenHandle TweenSet::after(FrameTime time, TweenCallback fn) {
    return start({}, time, {.on_finish = fn});
}

TweenHandle TweenSet::after(FrameTime time, FinishEvent event) {
    return start({}, time, {.publish = event});
}

TweenHandle TweenSet::every(FrameTime period, TweenCallback fn, int times) {
    if (period.valid() && period.ticks == 0) {
        detail::reject("every(): the period must be at least 1 tick");
        return {};
    }
    // Each loop end but the last calls on_loop, the last calls on_finish: the same function both times.
    return start({}, period, {.loops = times, .on_finish = fn, .on_loop = fn});
}

} // namespace nnh
