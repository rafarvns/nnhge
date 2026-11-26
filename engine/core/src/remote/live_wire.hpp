#pragma once
// Live value messages and records (spec 030, D51; additive to D28's version 1). Pure: shared with tools/nnh.
// Protocol: specs/030-live-values/plan.md "Remote protocol". Tests: tests/remote/test_live_wire.cpp.
//
// Messages (bytes 8–15; decoded into the 32-byte remote::Message's existing fields):
//   LiveSet (9)  hash u32 (≠ 0), value bits u32                       → press, release
//   Live (10)    op u8, arg u8, count u16, hash u32                    → screens, stick, id, press
//   Pause (11)   op u8, 0, steps u16 (step: 1..3600, else 0), 0 × 4    → screens, id
// Records (payloads): LiveEntry (8) 33..575 B, LiveRemoved (9) 8 B, LiveValue (10) 12..44 B, LiveRefused (11) 8 B,
// LiveState (12) 8 B. The game sends them only after a Live Catalogue request in its attachment.

#include <cstddef>
#include <cstdint>

#include "live/live_kinds.hpp"
#include "live/live_steps.hpp"
#include "remote/remote_wire.hpp"

namespace nnh::detail::remote {

enum class LiveOp : std::uint8_t { Catalogue = 1, Get = 2, Follow = 3, Unfollow = 4, Reset = 5, Dump = 6, Action = 7 };
/// The `arg` of a Live Reset.
enum class ResetScope : std::uint8_t { One = 0, Group = 1, All = 2 };
enum class PauseOp : std::uint8_t { Pause = 1, Resume = 2, Step = 3 };
inline constexpr std::uint16_t kMaxStepFrames = 3600;
/// Enum names in LiveEntry records: 1..15 bytes each.
inline constexpr int kMaxEnumNameBytes = 15;

// ---- messages ----------------------------------------------------------------------------------------------

Message live_set_message(std::uint32_t hash, std::uint32_t bits);
Message live_message(LiveOp op, std::uint8_t arg, std::uint16_t count, std::uint32_t hash);
Message pause_message(PauseOp op, std::uint16_t steps);

inline std::uint32_t live_hash(const Message& m) { return m.press; }
inline std::uint32_t live_bits(const Message& m) { return m.release; }
inline LiveOp live_op(const Message& m) { return static_cast<LiveOp>(m.screens); }
inline std::uint8_t live_arg(const Message& m) { return m.stick; }
inline std::uint16_t live_count(const Message& m) { return m.id; }
inline PauseOp pause_op(const Message& m) { return static_cast<PauseOp>(m.screens); }
inline std::uint16_t pause_steps(const Message& m) { return m.id; }

/// Decoders for remote_wire.cpp: bytes 8–15 of a LiveSet, Live or Pause message into `out`. False: Reject::Range.
bool decode_live_set_arguments(const std::uint8_t* a, Message& out);
bool decode_live_arguments(const std::uint8_t* a, Message& out);
bool decode_pause_arguments(const std::uint8_t* a, Message& out);
/// Encoders: bytes 8–15 (already zero) of those messages.
void encode_live_arguments(const Message& m, std::uint8_t* a);

// ---- records ------------------------------------------------------------------------------------------------

inline constexpr std::uint32_t kLiveEntryFixed = 32;
inline constexpr std::uint32_t kLiveEntryMin = kLiveEntryFixed + 1;
inline constexpr std::uint32_t kLiveEntryMax = kLiveEntryFixed + 31 + 32 * (1 + kMaxEnumNameBytes);
inline constexpr std::uint32_t kLiveRemovedPayload = 8;
inline constexpr std::uint32_t kLiveValueFixed = 12;
inline constexpr std::uint32_t kLiveValueMax = kLiveValueFixed + 32;
inline constexpr std::uint32_t kLiveRefusedPayload = 8;
inline constexpr std::uint32_t kLiveStatePayload = 8;

// LiveEntry flags
inline constexpr std::uint8_t kEntryTunable = 1u << 0;
inline constexpr std::uint8_t kEntryAction = 1u << 1;
inline constexpr std::uint8_t kEntryWatch = 1u << 2;
inline constexpr std::uint8_t kEntryShown = 1u << 3;   ///< a per-frame watch
inline constexpr std::uint8_t kEntryChanged = 1u << 4; ///< differs from its default
inline constexpr std::uint8_t kEntryPending = 1u << 5;
// LiveValue flags
inline constexpr std::uint8_t kValuePaused = 1u << 0;
inline constexpr std::uint8_t kValuePending = 1u << 1;
inline constexpr std::uint8_t kValueChangedThisFrame = 1u << 2;
inline constexpr std::uint8_t kValueStale = 1u << 3;
// LiveState flags
inline constexpr std::uint8_t kStateCatalogueComplete = 1u << 0;

struct LiveEntryHeader {
    std::uint32_t frame = 0, hash = 0;
    live::Kind kind = live::Kind::None;
    std::uint8_t flags = 0;
    std::uint8_t name_length = 0;
    std::uint8_t enum_count = 0;
    std::uint32_t value = 0, def = 0, min = 0, max = 0, step = 0;
};

/// A decoded LiveEntry: the header, the name (not NUL-terminated) and the enum names' bytes, inside the payload.
struct LiveEntryView {
    LiveEntryHeader header;
    const char* name = nullptr;
    const std::uint8_t* enum_names = nullptr; ///< enum_count × (u8 length + bytes)
};

/// The whole record (header included) into `out` (kRecordHeaderSize + kLiveEntryMax bytes); `enum_names` may be
/// null when `enum_count` is 0. Names are cut to their limits. @return its size.
std::size_t encode_live_entry(const LiveEntryHeader& h, const char* name, const char* const* enum_names,
                              int enum_count, std::uint8_t* out);
/// The record's fixed part only (header + 32 bytes), for a session that sends the name and enum names after it.
std::size_t encode_live_entry_fixed(const LiveEntryHeader& h, std::uint32_t payload_length, std::uint8_t* out);
/// Validates and splits a LiveEntry payload: a known kind, a valid name, enum names only for enums (1..32, 1..15 bytes
/// each), the exact length.
bool decode_live_entry(const std::uint8_t* payload, std::uint32_t length, LiveEntryView& out);
/// Enum name `index` of a decoded entry into `out` (16 bytes, NUL-terminated). @return its length, 0 if none.
int live_entry_enum_name(const LiveEntryView& v, int index, char out[kMaxEnumNameBytes + 1]);

struct LiveValueRecord {
    std::uint32_t frame = 0, hash = 0;
    live::Kind kind = live::Kind::None;
    std::uint8_t flags = 0;
    std::uint8_t length = 0; ///< value bytes: 4 (8 for Vec2), text 0..31
    std::uint8_t value[32] = {};
};
std::size_t encode_live_value(const LiveValueRecord& r, std::uint8_t* out); ///< kRecordHeaderSize + kLiveValueMax
bool decode_live_value(const std::uint8_t* payload, std::uint32_t length, LiveValueRecord& out);

struct LiveRefusedRecord {
    std::uint16_t seq = 0;
    live::Refusal reason = live::Refusal::None;
    std::uint32_t hash = 0;
};
std::size_t encode_live_refused(std::uint16_t seq, live::Refusal reason, std::uint32_t hash, std::uint8_t* out);
bool decode_live_refused(const std::uint8_t* payload, std::uint32_t length, LiveRefusedRecord& out);

struct LiveStateRecord {
    std::uint32_t frame = 0; ///< the next game frame
    bool paused = false;
    std::uint8_t flags = 0;
    std::uint16_t steps = 0; ///< steps left
};
std::size_t encode_live_state(std::uint32_t frame, bool paused, std::uint8_t flags, std::uint16_t steps,
                              std::uint8_t* out);
bool decode_live_state(const std::uint8_t* payload, std::uint32_t length, LiveStateRecord& out);

std::size_t encode_live_removed(std::uint32_t frame, std::uint32_t hash, std::uint8_t* out);
bool decode_live_removed(const std::uint8_t* payload, std::uint32_t length, std::uint32_t& frame, std::uint32_t& hash);

/// Payload length allowed for the live record types (8..12); false for other types.
bool live_record_length_ok(RecordType type, std::uint32_t length);

} // namespace nnh::detail::remote
