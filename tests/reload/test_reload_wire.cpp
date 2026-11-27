// Hot reload messages and records (spec 031 T1.1, FR-18, X, D54): exact bytes, round trips, payload lengths, every
// refusal (batch 0, files 0 and 257, total past 32 MiB, op 0 and 3, path length 0, size past 16 MiB, a remove with a
// size, chunk length 0 and 16385, frame ≠ 0 on a transfer message, non-zero reserved bytes), record lengths at their
// bounds and one past. Expected bytes are written by hand.

#include <cstdint>
#include <cstring>

#include "reload/reload_wire.hpp"
#include "remote/remote_wire.hpp"
#include "test.hpp"

using namespace nnh::detail::remote;

namespace {

/// A message of `type` with bytes 8..15 given, seq 7, frame `frame`.
void raw(std::uint8_t type, const std::uint8_t args[8], std::uint8_t out[16], std::uint32_t frame = 0) {
    const std::uint8_t head[8] = {0x52, type, 7, 0, static_cast<std::uint8_t>(frame), static_cast<std::uint8_t>(frame >> 8),
                                  static_cast<std::uint8_t>(frame >> 16), static_cast<std::uint8_t>(frame >> 24)};
    std::memcpy(out, head, 8);
    std::memcpy(out + 8, args, 8);
}

Reject decode_raw(std::uint8_t type, const std::uint8_t args[8], std::uint32_t frame = 0) {
    std::uint8_t bytes[16];
    raw(type, args, bytes, frame);
    Message m;
    return decode_message(bytes, m);
}

bool round_trips(const Message& m) {
    std::uint8_t bytes[16];
    encode_message(m, bytes);
    Message back;
    if (decode_message(bytes, back) != Reject::None) return false;
    std::uint8_t again[16];
    encode_message(back, again);
    return std::memcmp(bytes, again, 16) == 0;
}

} // namespace

NNH_TEST(reload_wire_begin_bytes) {
    Message m = reload_begin_message(0x0102, 256, 0x00ABCDEFu);
    m.seq = 7;
    std::uint8_t bytes[16];
    encode_message(m, bytes);
    const std::uint8_t expected[16] = {0x52, 12, 7, 0, 0, 0, 0, 0, 0x02, 0x01, 0x00, 0x01, 0xEF, 0xCD, 0xAB, 0x00};
    CHECK(std::memcmp(bytes, expected, 16) == 0);
    Message back;
    CHECK(decode_message(bytes, back) == Reject::None);
    CHECK(back.type == MessageType::ReloadBegin);
    CHECK(reload_batch(back) == 0x0102);
    CHECK(reload_files(back) == 256);
    CHECK(reload_total(back) == 0x00ABCDEFu);
    CHECK(payload_length(back) == 0);
}

NNH_TEST(reload_wire_file_bytes_and_payload) {
    Message m = reload_file_message(3, ReloadOp::Put, 12, 4096);
    std::uint8_t bytes[16];
    encode_message(m, bytes);
    const std::uint8_t args[8] = {3, 0, 1, 12, 0x00, 0x10, 0, 0};
    CHECK(bytes[1] == 13);
    CHECK(std::memcmp(bytes + 8, args, 8) == 0);
    Message back;
    CHECK(decode_message(bytes, back) == Reject::None);
    CHECK(reload_index(back) == 3);
    CHECK(reload_op(back) == ReloadOp::Put);
    CHECK(reload_path_length(back) == 12);
    CHECK(reload_size(back) == 4096);
    CHECK(payload_length(back) == 12 + 4); // the path and its CRC-32
    const Message remove = reload_file_message(4, ReloadOp::Remove, 255, 0);
    CHECK(round_trips(remove));
    CHECK(payload_length(remove) == 255); // the path only
}

NNH_TEST(reload_wire_chunk_bytes_and_payload) {
    const Message m = reload_chunk_message(2, 16384, 0x10000u);
    std::uint8_t bytes[16];
    encode_message(m, bytes);
    const std::uint8_t args[8] = {2, 0, 0x00, 0x40, 0x00, 0x00, 0x01, 0x00};
    CHECK(bytes[1] == 14);
    CHECK(std::memcmp(bytes + 8, args, 8) == 0);
    Message back;
    CHECK(decode_message(bytes, back) == Reject::None);
    CHECK(reload_chunk_length(back) == 16384);
    CHECK(reload_offset(back) == 0x10000u);
    CHECK(payload_length(back) == 16384);
    CHECK(payload_length(reload_chunk_message(0, 1, 0)) == 1);
}

NNH_TEST(reload_wire_apply_cancel_notice_bytes) {
    const Message apply = reload_apply_message(9, 2, 1234);
    std::uint8_t bytes[16];
    encode_message(apply, bytes);
    const std::uint8_t a1[16] = {0x52, 15, 0, 0, 0xD2, 0x04, 0, 0, 9, 0, 2, 0, 0, 0, 0, 0};
    CHECK(std::memcmp(bytes, a1, 16) == 0);
    CHECK(round_trips(apply));
    CHECK(payload_length(apply) == 0);

    const Message cancel = reload_cancel_message(0);
    encode_message(cancel, bytes);
    CHECK(bytes[1] == 16);
    CHECK(round_trips(cancel));
    CHECK(round_trips(reload_cancel_message(65535)));

    const Message notice = reload_notice_message(NoticeReason::NeedsRestart, 14, 3);
    encode_message(notice, bytes);
    const std::uint8_t a3[8] = {2, 14, 3, 0, 0, 0, 0, 0};
    CHECK(bytes[1] == 17);
    CHECK(std::memcmp(bytes + 8, a3, 8) == 0);
    Message back;
    CHECK(decode_message(bytes, back) == Reject::None);
    CHECK(notice_reason(back) == NoticeReason::NeedsRestart);
    CHECK(notice_others(back) == 3);
    CHECK(payload_length(back) == 14);
}

NNH_TEST(reload_wire_round_trips_at_the_limits) {
    CHECK(round_trips(reload_begin_message(1, 1, 0)));
    CHECK(round_trips(reload_begin_message(65535, 256, kReloadMaxBatchBytes)));
    CHECK(round_trips(reload_file_message(0, ReloadOp::Put, 1, kReloadMaxFileBytes)));
    CHECK(round_trips(reload_file_message(255, ReloadOp::Put, 255, 0)));
    CHECK(round_trips(reload_chunk_message(255, 1, kReloadMaxFileBytes - 1)));
    CHECK(round_trips(reload_apply_message(1, 256, 0)));
    CHECK(round_trips(reload_notice_message(NoticeReason::ConversionFailed, 1, 65535)));
}

NNH_TEST(reload_wire_refuses_begin_out_of_range) {
    const std::uint8_t batch0[8] = {0, 0, 1, 0, 0, 0, 0, 0};
    CHECK(decode_raw(12, batch0) == Reject::Range);
    const std::uint8_t files0[8] = {1, 0, 0, 0, 0, 0, 0, 0};
    CHECK(decode_raw(12, files0) == Reject::Range);
    const std::uint8_t files257[8] = {1, 0, 0x01, 0x01, 0, 0, 0, 0};
    CHECK(decode_raw(12, files257) == Reject::Range);
    const std::uint8_t total_over[8] = {1, 0, 1, 0, 0x01, 0x00, 0x00, 0x02}; // 32 MiB + 1
    CHECK(decode_raw(12, total_over) == Reject::Range);
    const std::uint8_t total_max[8] = {1, 0, 1, 0, 0x00, 0x00, 0x00, 0x02};
    CHECK(decode_raw(12, total_max) == Reject::None);
}

NNH_TEST(reload_wire_refuses_file_out_of_range) {
    const std::uint8_t op0[8] = {0, 0, 0, 4, 1, 0, 0, 0};
    CHECK(decode_raw(13, op0) == Reject::Range);
    const std::uint8_t op3[8] = {0, 0, 3, 4, 1, 0, 0, 0};
    CHECK(decode_raw(13, op3) == Reject::Range);
    const std::uint8_t path0[8] = {0, 0, 1, 0, 1, 0, 0, 0};
    CHECK(decode_raw(13, path0) == Reject::Range);
    const std::uint8_t size_over[8] = {0, 0, 1, 4, 0x01, 0x00, 0x00, 0x01}; // 16 MiB + 1
    CHECK(decode_raw(13, size_over) == Reject::Range);
    const std::uint8_t size_max[8] = {0, 0, 1, 4, 0x00, 0x00, 0x00, 0x01};
    CHECK(decode_raw(13, size_max) == Reject::None);
    const std::uint8_t remove_with_size[8] = {0, 0, 2, 4, 1, 0, 0, 0};
    CHECK(decode_raw(13, remove_with_size) == Reject::Range);
    const std::uint8_t index_over[8] = {0x00, 0x01, 1, 4, 1, 0, 0, 0}; // index 256
    CHECK(decode_raw(13, index_over) == Reject::Range);
}

NNH_TEST(reload_wire_refuses_chunk_out_of_range) {
    const std::uint8_t len0[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    CHECK(decode_raw(14, len0) == Reject::Range);
    const std::uint8_t len_over[8] = {0, 0, 0x01, 0x40, 0, 0, 0, 0}; // 16385
    CHECK(decode_raw(14, len_over) == Reject::Range);
    const std::uint8_t offset_over[8] = {0, 0, 1, 0, 0x00, 0x00, 0x00, 0x01}; // offset 16 MiB
    CHECK(decode_raw(14, offset_over) == Reject::Range);
    const std::uint8_t index_over[8] = {0x00, 0x01, 1, 0, 0, 0, 0, 0};
    CHECK(decode_raw(14, index_over) == Reject::Range);
}

NNH_TEST(reload_wire_refuses_apply_cancel_notice_out_of_range) {
    const std::uint8_t apply_batch0[8] = {0, 0, 1, 0, 0, 0, 0, 0};
    CHECK(decode_raw(15, apply_batch0) == Reject::Range);
    const std::uint8_t apply_files0[8] = {1, 0, 0, 0, 0, 0, 0, 0};
    CHECK(decode_raw(15, apply_files0) == Reject::Range);
    const std::uint8_t apply_reserved[8] = {1, 0, 1, 0, 0, 0, 0, 1};
    CHECK(decode_raw(15, apply_reserved) == Reject::Range);
    const std::uint8_t cancel_reserved[8] = {1, 0, 1, 0, 0, 0, 0, 0};
    CHECK(decode_raw(16, cancel_reserved) == Reject::Range);
    const std::uint8_t notice0[8] = {0, 4, 0, 0, 0, 0, 0, 0};
    CHECK(decode_raw(17, notice0) == Reject::Range);
    const std::uint8_t notice3[8] = {3, 4, 0, 0, 0, 0, 0, 0};
    CHECK(decode_raw(17, notice3) == Reject::Range);
    const std::uint8_t notice_path0[8] = {1, 0, 0, 0, 0, 0, 0, 0};
    CHECK(decode_raw(17, notice_path0) == Reject::Range);
    const std::uint8_t notice_reserved[8] = {1, 4, 0, 0, 1, 0, 0, 0};
    CHECK(decode_raw(17, notice_reserved) == Reject::Range);
}

NNH_TEST(reload_wire_refuses_reserved_bytes_of_begin_file_chunk) {
    // Begin has no reserved byte; File and Chunk have none either; a non-zero frame is the only extra refusal.
    const std::uint8_t begin[8] = {1, 0, 1, 0, 0, 0, 0, 0};
    CHECK(decode_raw(12, begin, 0) == Reject::None);
    CHECK(decode_raw(12, begin, 5) == Reject::Range);
    const std::uint8_t file[8] = {0, 0, 1, 4, 1, 0, 0, 0};
    CHECK(decode_raw(13, file, 1) == Reject::Range);
    const std::uint8_t chunk[8] = {0, 0, 1, 0, 0, 0, 0, 0};
    CHECK(decode_raw(14, chunk, 1) == Reject::Range);
    const std::uint8_t cancel[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    CHECK(decode_raw(16, cancel, 1) == Reject::Range);
    const std::uint8_t notice[8] = {1, 4, 0, 0, 0, 0, 0, 0};
    CHECK(decode_raw(17, notice, 1) == Reject::Range);
    const std::uint8_t apply[8] = {1, 0, 1, 0, 0, 0, 0, 0};
    CHECK(decode_raw(15, apply, 99) == Reject::None); // Apply is the frame-stamped one
}

NNH_TEST(reload_wire_transfer_types) {
    CHECK(is_reload_transfer(MessageType::ReloadBegin));
    CHECK(is_reload_transfer(MessageType::ReloadFile));
    CHECK(is_reload_transfer(MessageType::ReloadChunk));
    CHECK(!is_reload_transfer(MessageType::ReloadApply));
    CHECK(is_reload_transfer(MessageType::ReloadCancel));
    CHECK(is_reload_transfer(MessageType::ReloadNotice));
    CHECK(!is_reload_transfer(MessageType::Pause));
    CHECK(payload_length(Message{}) == 0);
}

NNH_TEST(reload_wire_staged_record_bytes) {
    std::uint8_t out[kMaxReloadRecord];
    const std::size_t n = encode_reload_staged({.batch = 3, .files = 2, .frame = 0x01020304u}, out);
    const std::uint8_t expected[16] = {0x1E, 'N', 'R', 13, 8, 0, 0, 0, 3, 0, 2, 0, 4, 3, 2, 1};
    CHECK(n == 16);
    CHECK(std::memcmp(out, expected, 16) == 0);
    RecordHeader h;
    CHECK(decode_record_header(out, h));
    CHECK(h.type == RecordType::ReloadStaged);
    ReloadStagedRecord back;
    CHECK(decode_reload_staged(out + kRecordHeaderSize, h.length, back));
    CHECK(back.batch == 3 && back.files == 2 && back.frame == 0x01020304u);
}

NNH_TEST(reload_wire_applied_record_bytes) {
    std::uint8_t out[kMaxReloadRecord];
    const std::size_t n =
        encode_reload_applied({.batch = 1, .files = 4, .frame = 100, .count = 7, .micros = 0x00010203u}, out);
    const std::uint8_t expected[24] = {0x1E, 'N', 'R', 14, 16, 0, 0, 0, 1, 0, 4, 0, 100, 0, 0, 0,
                                       7, 0, 0, 0, 3, 2, 1, 0};
    CHECK(n == 24);
    CHECK(std::memcmp(out, expected, 24) == 0);
    ReloadAppliedRecord back;
    CHECK(decode_reload_applied(out + kRecordHeaderSize, 16, back));
    CHECK(back.count == 7 && back.micros == 0x00010203u && back.files == 4);
}

NNH_TEST(reload_wire_refused_record_bytes) {
    std::uint8_t out[kMaxReloadRecord];
    const ReloadRefusedRecord r{.batch = 2, .stage = ReloadStage::Memory, .reason = ReloadReason::NoMemory, .file = 1,
                                .frame = 50, .missing = 4194304};
    const std::size_t n = encode_reload_refused(r, out);
    const std::uint8_t expected[24] = {0x1E, 'N', 'R', 15, 16, 0, 0, 0, 2, 0, 3, 22, 1, 0, 0, 0,
                                       50, 0, 0, 0, 0, 0, 0x40, 0};
    CHECK(n == 24);
    CHECK(std::memcmp(out, expected, 24) == 0);
    ReloadRefusedRecord back;
    CHECK(decode_reload_refused(out + kRecordHeaderSize, 16, back));
    CHECK(back.stage == ReloadStage::Memory && back.reason == ReloadReason::NoMemory && back.missing == 4194304);
}

NNH_TEST(reload_wire_record_lengths_at_their_bounds) {
    CHECK(reload_record_length_ok(RecordType::ReloadStaged, 8));
    CHECK(!reload_record_length_ok(RecordType::ReloadStaged, 7));
    CHECK(!reload_record_length_ok(RecordType::ReloadStaged, 9));
    CHECK(reload_record_length_ok(RecordType::ReloadApplied, 16));
    CHECK(!reload_record_length_ok(RecordType::ReloadApplied, 17));
    CHECK(reload_record_length_ok(RecordType::ReloadRefused, 16));
    CHECK(!reload_record_length_ok(RecordType::ReloadRefused, 15));
    CHECK(!reload_record_length_ok(RecordType::LiveState, 8));
    std::uint8_t header[kRecordHeaderSize];
    RecordHeader h;
    encode_record_header(RecordType::ReloadApplied, 15, header);
    CHECK(!decode_record_header(header, h));
    encode_record_header(RecordType::ReloadRefused, 16, header);
    CHECK(decode_record_header(header, h));
    header[3] = 16; // past the last record type
    CHECK(!decode_record_header(header, h));
}

NNH_TEST(reload_wire_record_decoders_refuse_bad_fields) {
    std::uint8_t out[kMaxReloadRecord];
    encode_reload_refused({.batch = 1, .stage = ReloadStage::Transfer, .reason = ReloadReason::Crc}, out);
    ReloadRefusedRecord back;
    out[kRecordHeaderSize + 2] = 0; // stage 0
    CHECK(!decode_reload_refused(out + kRecordHeaderSize, 16, back));
    out[kRecordHeaderSize + 2] = 8; // stage 8
    CHECK(!decode_reload_refused(out + kRecordHeaderSize, 16, back));
    out[kRecordHeaderSize + 2] = 1;
    out[kRecordHeaderSize + 6] = 1; // reserved u16
    CHECK(!decode_reload_refused(out + kRecordHeaderSize, 16, back));
    ReloadStagedRecord staged;
    CHECK(!decode_reload_staged(out + kRecordHeaderSize, 9, staged));
    ReloadAppliedRecord applied;
    CHECK(!decode_reload_applied(out + kRecordHeaderSize, 8, applied));
}

NNH_TEST(reload_wire_reason_texts) {
    CHECK(std::strcmp(reload_reason_text(ReloadReason::Crc), "bad CRC") == 0);
    CHECK(std::strcmp(reload_reason_text(ReloadReason::Order), "out of order") == 0);
    CHECK(std::strcmp(reload_reason_text(static_cast<ReloadReason>(200)), "?") == 0);
}
