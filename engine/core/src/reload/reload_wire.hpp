#pragma once
// Hot reload messages and records (spec 031, D54; additive to D28's version 1). Pure: shared with tools/nnh.
// Protocol: specs/031-hot-reload-assets/plan.md "Remote protocol". Tests: tests/reload/test_reload_wire.cpp.
//
// Messages (bytes 8–15; decoded into the 32-byte remote::Message's existing fields):
//   ReloadBegin (12)  batch u16 (≠ 0), files u16 (1..256), total u32 (≤ 32 MiB)          → id, x, press
//   ReloadFile (13)   index u16, op u8 (1 put, 2 remove), path length u8 (1..255), size u32 → id, screens, x, press
//   ReloadChunk (14)  index u16, length u16 (1..16384), offset u32                          → id, x, press
//   ReloadApply (15)  batch u16 (≠ 0), files u16 (1..256), 0 × 4                             → id, x
//   ReloadCancel (16) batch u16 (0 = whatever is open or staged), 0 × 6                     → id
//   ReloadNotice (17) reason u8 (1, 2), path length u8 (1..255), others u16, 0 × 4         → screens, x, release
// Every message but Apply must carry frame 0 (they are handled at receipt). Payloads (raw bytes after the message):
// File → the path, then (put) its CRC-32 u32; Chunk → `length` bytes; Notice → the path.
// Records (payloads): ReloadStaged (13) 8 B, ReloadApplied (14) 16 B, ReloadRefused (15) 16 B. The game sends them only
// after the PC sent a ReloadBegin or ReloadNotice in this attachment.

#include <cstddef>
#include <cstdint>

#include "remote/remote_wire.hpp"

namespace nnh::detail::remote {

inline constexpr std::uint16_t kReloadMaxFiles = 256;
inline constexpr std::uint32_t kReloadMaxFileBytes = 16u << 20;
inline constexpr std::uint32_t kReloadMaxBatchBytes = 32u << 20;
inline constexpr std::uint16_t kReloadMaxChunk = 16384;
inline constexpr int kReloadMaxPath = 255;

enum class ReloadOp : std::uint8_t { Put = 1, Remove = 2 };
enum class NoticeReason : std::uint8_t { ConversionFailed = 1, NeedsRestart = 2 };

/// The stage of a ReloadRefused record.
enum class ReloadStage : std::uint8_t {
    Transfer = 1, Validate = 2, Memory = 3, Restart = 4, Cancelled = 5, Busy = 6, Notice = 7,
};

/// The reason byte of a ReloadRefused record (refines the stage).
enum class ReloadReason : std::uint8_t {
    None = 0,
    // transfer (the receiver)
    Order = 1,         ///< a file or chunk out of order, a chunk without an open file
    Offset = 2,        ///< a chunk's offset is not the bytes received so far
    Size = 3,          ///< a file longer or shorter than announced
    Total = 4,         ///< the batch's bytes don't add up to its total
    Crc = 5,           ///< a put's bytes don't match its CRC-32
    Path = 6,          ///< not a valid asset path
    Batch = 7,         ///< Apply for another batch or file count, or nothing staged
    Write = 8,         ///< the file could not be written
    Disconnected = 9,  ///< the PC went away mid-batch
    // validate / memory / restart (the batch engine and the kinds)
    Corrupted = 20,    ///< the loader's validator refused the file
    Consistency = 21,  ///< puts and removes don't match the batch's asset index
    NoMemory = 22,     ///< an allocation, slot or capacity ran out
    Schema = 23,       ///< a data asset made for another struct (D29's fingerprint)
    Languages = 24,    ///< a string manifest with another language list
    TooManyPaths = 25, ///< the overlay is full for this session
    // notices (copied from the ReloadNotice)
    ConversionFailed = 30,
    NeedsRestart = 31,
};

// ---- messages ----------------------------------------------------------------------------------------------

Message reload_begin_message(std::uint16_t batch, std::uint16_t files, std::uint32_t total);
Message reload_file_message(std::uint16_t index, ReloadOp op, std::uint8_t path_length, std::uint32_t size);
Message reload_chunk_message(std::uint16_t index, std::uint16_t length, std::uint32_t offset);
Message reload_apply_message(std::uint16_t batch, std::uint16_t files, std::uint32_t frame);
Message reload_cancel_message(std::uint16_t batch);
Message reload_notice_message(NoticeReason reason, std::uint8_t path_length, std::uint16_t others);

inline std::uint16_t reload_batch(const Message& m) { return m.id; }      ///< Begin, Apply, Cancel
inline std::uint16_t reload_index(const Message& m) { return m.id; }      ///< File, Chunk
inline std::uint16_t reload_files(const Message& m) { return static_cast<std::uint16_t>(m.x); } ///< Begin, Apply
inline std::uint32_t reload_total(const Message& m) { return m.press; }   ///< Begin
inline ReloadOp reload_op(const Message& m) { return static_cast<ReloadOp>(m.screens); }
inline std::uint8_t reload_path_length(const Message& m) { return static_cast<std::uint8_t>(m.x); } ///< File, Notice
inline std::uint32_t reload_size(const Message& m) { return m.press; }    ///< File
inline std::uint16_t reload_chunk_length(const Message& m) { return static_cast<std::uint16_t>(m.x); }
inline std::uint32_t reload_offset(const Message& m) { return m.press; }  ///< Chunk
inline NoticeReason notice_reason(const Message& m) { return static_cast<NoticeReason>(m.screens); }
inline std::uint16_t notice_others(const Message& m) { return static_cast<std::uint16_t>(m.release); }

/// True for the transfer messages (12–14, 16, 17), which carry frame 0 and are handled at receipt.
bool is_reload_transfer(MessageType type);

/// Raw bytes that follow a decoded message on the stream (0 for every message without a payload). A pure function
/// of the decoded fields, bounded by the type's limits.
std::uint32_t payload_length(const Message& m);

/// Decoder for remote_wire.cpp: frame and bytes 8–15 of a reload message into `out`. False: Reject::Range.
bool decode_reload_arguments(const std::uint8_t* in, Message& out);
/// Encoder: bytes 8–15 (already zero) of a reload message.
void encode_reload_arguments(const Message& m, std::uint8_t* a);

// ---- records ------------------------------------------------------------------------------------------------

inline constexpr std::uint32_t kReloadStagedPayload = 8;
inline constexpr std::uint32_t kReloadAppliedPayload = 16;
inline constexpr std::uint32_t kReloadRefusedPayload = 16;
/// The largest reload record, header included.
inline constexpr std::size_t kMaxReloadRecord = kRecordHeaderSize + 16;
inline constexpr std::uint16_t kNoFile = 0xFFFF;

struct ReloadStagedRecord {
    std::uint16_t batch = 0, files = 0;
    std::uint32_t frame = 0;
};
struct ReloadAppliedRecord {
    std::uint16_t batch = 0, files = 0;
    std::uint32_t frame = 0, count = 0, micros = 0;
};
struct ReloadRefusedRecord {
    std::uint16_t batch = 0;
    ReloadStage stage = ReloadStage::Transfer;
    ReloadReason reason = ReloadReason::None;
    std::uint16_t file = kNoFile;
    std::uint32_t frame = 0, missing = 0;
};

std::size_t encode_reload_staged(const ReloadStagedRecord& r, std::uint8_t* out);
std::size_t encode_reload_applied(const ReloadAppliedRecord& r, std::uint8_t* out);
std::size_t encode_reload_refused(const ReloadRefusedRecord& r, std::uint8_t* out);
bool decode_reload_staged(const std::uint8_t* payload, std::uint32_t length, ReloadStagedRecord& out);
bool decode_reload_applied(const std::uint8_t* payload, std::uint32_t length, ReloadAppliedRecord& out);
bool decode_reload_refused(const std::uint8_t* payload, std::uint32_t length, ReloadRefusedRecord& out);

/// Payload length allowed for the reload record types (13..15); false for other types.
bool reload_record_length_ok(RecordType type, std::uint32_t length);

/// Short text of a refusal reason, for logs ("chunk out of order", "bad CRC", ...).
const char* reload_reason_text(ReloadReason reason);

} // namespace nnh::detail::remote
