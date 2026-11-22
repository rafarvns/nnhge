#pragma once
// Functions shared by the tween module's files (spec 015). Not used by other modules.

#include "nnh/log.hpp"
#include "nnh/memory.hpp"
#include "tween/tween_block.hpp"

NNH_LOG_CHANNEL_EXTERN(tween);

namespace nnh::detail {

/// The memory tag of every set's block ("tween").
MemTag& tween_memory_tag();

/// The slot `h` refers to, or -1 when stale or empty. A handle of another set asserts (and is ignored in
/// Release, with a log).
int tween_find(const TweenBlock* block, TweenHandle h);

/// Ends a slot without callbacks: its handles go stale now; it is freed now, or by the sweep when `busy`.
void tween_kill_slot(TweenBlock& block, int index);
/// Unlinks and frees the Dead slots (when the block is no longer busy).
void tween_sweep(TweenBlock& block);

/// Starts a slot: the end of its delay. Captures the start value of `to`/`by` tweens from the pointer target.
void tween_begin(SlotHot& slot);
/// Advances one Active slot by `ticks` (delay, start, loops, yoyo, carry, finish), writing its value.
void tween_advance_slot(TweenBlock& block, int index, std::int32_t ticks);
/// Finishes a slot: writes its final value (`backwards_end`: the start value), ends it, then runs on_finish
/// and queues its event (FR-11), unless the callback depth limit is reached.
void tween_finish(TweenBlock& block, int index, bool backwards_end);
/// Runs a slot's on_loop callback (depth limit applies).
void tween_loop_callback(TweenBlock& block, int index);
/// Marks the block busy around callbacks; leaving the outermost section sweeps the Dead slots.
void tween_enter(TweenBlock& block);
void tween_leave(TweenBlock& block);

// Values (tween_values.cpp): `count` components per kind, Color as 0..255 floats.
int tween_components(TweenKind kind);
void tween_pack(TweenKind kind, const void* value, float out[4]);
/// Writes `v` to the slot's pointer or setter target (Color: clamped to 0–255, rounded to nearest).
void tween_write(const SlotHot& slot, const float v[4]);
/// The value at the slot's current time (yoyo backwards: mirrored time), written to its target.
void tween_write_current(const SlotHot& slot);

} // namespace nnh::detail
