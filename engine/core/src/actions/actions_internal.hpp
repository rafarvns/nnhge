#pragma once
// Functions shared by the files of the actions module (spec 017). Internal.

#include "actions/action_block.hpp"
#include "actions/input_view.hpp"
#include "nnh/log.hpp"
#include "nnh/memory.hpp"

#include <cstdint>

NNH_LOG_CHANNEL_EXTERN(actions);

namespace nnh::detail {

MemTag& actions_memory_tag();

// ---- Sources and slots (source_eval.cpp, action_combine.cpp; pure) ------------------------------------------

/// One slot's reading this frame.
struct SlotOut {
    bool held = false; ///< button actions: the slot holds the action
    float value = 0;   ///< button: 0..1; axis: -1..1; vector: its length
    Vec2 vec{};        ///< vector actions; button/axis: (value, 0)
};

/// A threshold in 1 % steps (0..100), rounded.
std::uint8_t to_percent(float v);
/// Whether a source kind can be bound to an action kind (touch included: button actions).
bool source_fits(ActionKind kind, Source::Kind source);
/// Whether `part` names a composite part of an action of `kind`.
bool part_fits(ActionKind kind, Part part);
/// Whether the data bytes are in range for the kind (buttons 0..13, composite parts 0..13 or kNoButton, sticks
/// 0..1, directions 0..3, axes 0..1, unused bytes 0). Touch: always true (its rectangle is not checked).
bool source_data_valid(const PackedSource& s);
PackedSource pack(const Source& s);
Source unpack(const SlotRec& slot);
/// A declared binding as a slot (defaults = current); kActive from the capability bits `caps`.
SlotRec make_slot(const Binding& binding, std::uint8_t caps);
/// Recomputes kActive of a slot after its source changed (FR-9).
void refresh_slot(SlotRec& slot, std::uint8_t caps);

/// The slot's reading. Stick directions in button actions use and update the slot's hysteresis bit.
SlotOut eval_slot(ActionKind kind, SlotRec& slot, const InputView& view);
/// Whether the slot's control is engaged (any of its buttons down, touched inside, a stick at least at
/// `threshold`): used to block slots when a context is enabled and to unblock them.
bool slot_engaged(const SlotRec& slot, const InputView& view, float threshold);

/// FR-7: folds one slot into the action's reading (any-held and max value for buttons; the largest |value|
/// for axes; the longest vector; ties keep the earlier slot).
void combine(ActionKind kind, SlotOut& into, const SlotOut& slot);
/// Hysteresis of axis/vector held: on at `press`, kept until below `release`.
bool held_with_hysteresis(float magnitude, bool was_held, float press, float release);
/// An action's slots read and combined (FR-6, FR-7), blocked slots skipped or unblocked (FR-13): eval_slot() and
/// combine() over its active slots, fast (read_slots.cpp). Button/axis vectors are (value, 0).
SlotOut read_slots(ActionRec& action, const InputView& view);

// ---- The set (action_*.cpp) -----------------------------------------------------------------------------------

/// The record of `id`, or nullptr (asserts; Release logs) when the id is out of range or not declared.
ActionRec* find_action(ActionBlock* block, ActionId id);
const ActionRec* find_action(const ActionBlock* block, ActionId id);
/// The slot, or nullptr (asserts / logs) when the action or slot is invalid.
SlotRec* find_slot(ActionBlock* block, ActionId id, int slot);

/// The context's effective state now (bound: its scene is the top scene).
bool context_on(const ActionBlock& block, int context);
/// Ends a scene binding (detaches the node); the context is disabled.
void context_unbind(ActionBlock& block, int context);

/// Registers / removes the set's after-input hook (D32; action_hook.cpp).
void actions_attach(ActionBlock& block);
void actions_detach(ActionBlock& block);

/// One evaluation (FR-11..FR-15): contexts, blocking, listening, slots, edges, counters, repeat.
void evaluate(ActionBlock& block, const InputView& view);
/// Queues this evaluation's ActionPressed then ActionReleased, in declaration order (FR-14).
void publish_action_events(const ActionBlock& block);
/// One evaluation while listening (FR-16): timeout, cancel, capture.
void listen_step(ActionBlock& block, const InputView& view);

/// Puts `source` into a binding unit with the conflict policy (FR-17): a whole source when `part` is Whole, else a
/// Button source for that composite part. Checked by the caller: the slot is remappable and the source fits.
RemapResult assign(ActionBlock& block, int action, int slot, Part part, const Source& source,
                   ConflictPolicy policy);
/// Why `source` can't go into the unit by remapping (locked slot, touch, wrong kind, bad part), or nullptr.
const char* remap_problem(const ActionRec& action, int slot, Part part, const Source& source);
/// The controls a binding unit reads: Button bits, plus stick directions at bit 16 + stick * 4 + direction.
std::uint32_t footprint(const PackedSource& source, Part part);
/// The binding units of a slot: its composite parts, or Whole; none when empty. Returns the count.
int units_of(const PackedSource& source, Part out[4]);
/// Index of a composite part in the source's data bytes (Negative/Up 0, Positive/Down 1, Left 2, Right 3).
int part_index(Part part);

/// Clears the runtime state of a slot whose source changed.
inline void slot_changed(SlotRec& slot, std::uint8_t caps) {
    slot.state = 0;
    refresh_slot(slot, caps);
}

} // namespace nnh::detail
