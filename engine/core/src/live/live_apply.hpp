#pragma once
// The application point (spec 030 FR-3, FR-4, FR-5, D51): panel edits and remote operations wait as pending values and
// are applied at the start of a game frame, in name order; a tunable whose bits change gets changed_frame and its
// callback; actions run there. Nothing is walked while nothing waits (FR-19). Tests: tests/live/test_live_apply.cpp.

#include <cstdint>

#include "live/live_block.hpp"
#include "live/live_steps.hpp"

namespace nnh::detail::live {

/// The variable's current bits (float bits, int32, bool 0/1, packed colour, enum index).
std::uint32_t read_bits(const Entry& e);
/// Writes bits into the variable (the game's own type and size).
void write_bits(Entry& e, std::uint32_t bits);
/// What a value is checked against.
ValueRule rule_of(const Entry& e);

/// A panel edit: the entry's pending value (latest wins).
void set_pending(Registry& r, int slot, std::uint32_t bits);
/// A panel press on an action.
void request_run(Registry& r, int slot);
/// A remote operation for the application point. False (refused: QueueFull) when kMaxOps are waiting.
bool queue_op(Registry& r, const RemoteOp& op);

/// The application point for game frame `frame`.
void frame_start(std::uint32_t frame);

/// The default written now, without changed() or a callback (game code: Handle::reset, reset_all, reset_group).
void reset_now(Entry& e);

/// Called for a refused remote operation (live_remote.cpp sets it); counted either way.
extern void (*g_on_refused)(std::uint16_t seq, Refusal reason, std::uint32_t hash);
void count_refusal(Registry& r, std::uint16_t seq, Refusal reason, std::uint32_t hash);

} // namespace nnh::detail::live
