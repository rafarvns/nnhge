// Live value messages and records (spec 030 T1.5, FR-11, FR-18, X): round trips through the 16-byte messages, every
// refusal (hash 0 where not allowed, op/arg/count ranges, step 0 or 3601, non-zero reserved bytes), record lengths at
// their bounds and one past. Expected bytes are written by hand.

#include <cstdint>
#include <cstring>

#include "remote/live_wire.hpp"
#include "remote/remote_wire.hpp"
#include "test.hpp"

using namespace nnh::detail::remote;
namespace live = nnh::detail::live;

namespace {

Reject decode(const std::uint8_t* bytes, Message& out) { return decode_message(bytes, out); }

/// A message of `type` with bytes 8..15 given, seq 7, frame 0x01020304.
void raw(std::uint8_t type, const std::uint8_t args[8], std::uint8_t out[16]) {
    const std::uint8_t head[8] = {0x52, type, 7, 0, 4, 3, 2, 1};
    std::memcpy(out, head, 8);
    std::memcpy(out + 8, args, 8);
}

bool round_trips(const Message& m) {
    std::uint8_t bytes[16];
    encode_message(m, bytes);
    Message back;
    if (decode(bytes, back) != Reject::None) return false;
    std::uint8_t again[16];
    encode_message(back, again);
    return std::memcmp(bytes, again, 16) == 0;
}

} // namespace

NNH_TEST(live_wire_live_set_bytes) {
    Message m = live_set_message(0xA1B2C3D4u, 0x40A80000u); // 5.25f
    m.seq = 7;
    m.frame = 0x01020304u;
    std::uint8_t bytes[16];
    encode_message(m, bytes);
    const std::uint8_t expected[16] = {0x52, 9, 7, 0, 4, 3, 2, 1, 0xD4, 0xC3, 0xB2, 0xA1, 0x00, 0x00, 0xA8, 0x40};
    CHECK(std::memcmp(bytes, expected, 16) == 0);
    Message back;
    CHECK(decode(bytes, back) == Reject::None);
    CHECK(back.type == MessageType::LiveSet);
    CHECK(live_hash(back) == 0xA1B2C3D4u);
    CHECK(live_bits(back) == 0x40A80000u);
}

NNH_TEST(live_wire_live_set_refuses_hash_zero) {
    const std::uint8_t args[8] = {0, 0, 0, 0, 1, 0, 0, 0};
    std::uint8_t bytes[16];
    raw(9, args, bytes);
    Message m;
    CHECK(decode(bytes, m) == Reject::Range);
}

NNH_TEST(live_wire_live_message_bytes) {
    Message m = live_message(LiveOp::Follow, 5, 600, 0x11223344u);
    std::uint8_t bytes[16];
    encode_message(m, bytes);
    const std::uint8_t args[8] = {3, 5, 0x58, 0x02, 0x44, 0x33, 0x22, 0x11};
    CHECK(std::memcmp(bytes + 8, args, 8) == 0);
    CHECK(bytes[1] == 10);
    Message back;
    CHECK(decode(bytes, back) == Reject::None);
    CHECK(live_op(back) == LiveOp::Follow);
    CHECK(live_arg(back) == 5);
    CHECK(live_count(back) == 600);
    CHECK(live_hash(back) == 0x11223344u);
}

NNH_TEST(live_wire_live_messages_round_trip) {
    CHECK(round_trips(live_message(LiveOp::Catalogue, 0, 0, 0)));
    CHECK(round_trips(live_message(LiveOp::Get, 0, 0, 9)));
    CHECK(round_trips(live_message(LiveOp::Follow, 1, 0, 9)));
    CHECK(round_trips(live_message(LiveOp::Follow, 255, 65535, 9)));
    CHECK(round_trips(live_message(LiveOp::Unfollow, 0, 0, 0)));
    CHECK(round_trips(live_message(LiveOp::Unfollow, 0, 0, 9)));
    CHECK(round_trips(live_message(LiveOp::Reset, 0, 0, 9)));
    CHECK(round_trips(live_message(LiveOp::Reset, 1, 0, 9)));
    CHECK(round_trips(live_message(LiveOp::Reset, 2, 0, 0)));
    CHECK(round_trips(live_message(LiveOp::Dump, 0, 0, 0)));
    CHECK(round_trips(live_message(LiveOp::Action, 0, 0, 9)));
}

NNH_TEST(live_wire_live_messages_refuse_bad_fields) {
    struct Case {
        std::uint8_t args[8];
    };
    const Case bad[] = {
        {{0, 0, 0, 0, 0, 0, 0, 0}},          // op 0
        {{8, 0, 0, 0, 9, 0, 0, 0}},          // op 8
        {{1, 0, 0, 0, 9, 0, 0, 0}},          // Catalogue with a hash
        {{1, 1, 0, 0, 0, 0, 0, 0}},          // Catalogue with an arg
        {{2, 0, 0, 0, 0, 0, 0, 0}},          // Get without a hash
        {{2, 0, 1, 0, 9, 0, 0, 0}},          // Get with a count
        {{3, 0, 0, 0, 9, 0, 0, 0}},          // Follow every 0
        {{3, 1, 0, 0, 0, 0, 0, 0}},          // Follow without a hash
        {{4, 1, 0, 0, 9, 0, 0, 0}},          // Unfollow with an arg
        {{5, 3, 0, 0, 9, 0, 0, 0}},          // Reset scope 3
        {{5, 2, 0, 0, 9, 0, 0, 0}},          // Reset all with a hash
        {{5, 0, 0, 0, 0, 0, 0, 0}},          // Reset one without a hash
        {{5, 1, 0, 0, 0, 0, 0, 0}},          // Reset group without a hash
        {{6, 0, 0, 0, 9, 0, 0, 0}},          // Dump with a hash
        {{7, 0, 0, 0, 0, 0, 0, 0}},          // Action without a hash
        {{7, 0, 0, 1, 9, 0, 0, 0}},          // Action with a count
    };
    for (const Case& c : bad) {
        std::uint8_t bytes[16];
        raw(10, c.args, bytes);
        Message m;
        CHECK(decode(bytes, m) == Reject::Range);
    }
}

NNH_TEST(live_wire_pause_messages) {
    CHECK(round_trips(pause_message(PauseOp::Pause, 0)));
    CHECK(round_trips(pause_message(PauseOp::Resume, 0)));
    CHECK(round_trips(pause_message(PauseOp::Step, 1)));
    CHECK(round_trips(pause_message(PauseOp::Step, 3600)));
    Message m = pause_message(PauseOp::Step, 3);
    std::uint8_t bytes[16];
    encode_message(m, bytes);
    const std::uint8_t args[8] = {3, 0, 3, 0, 0, 0, 0, 0};
    CHECK(bytes[1] == 11 && std::memcmp(bytes + 8, args, 8) == 0);
    Message back;
    CHECK(decode(bytes, back) == Reject::None);
    CHECK(pause_op(back) == PauseOp::Step && pause_steps(back) == 3);
}

NNH_TEST(live_wire_pause_refuses_bad_fields) {
    const std::uint8_t bad[][8] = {
        {0, 0, 0, 0, 0, 0, 0, 0},       // op 0
        {4, 0, 0, 0, 0, 0, 0, 0},       // op 4
        {3, 0, 0, 0, 0, 0, 0, 0},       // step 0
        {3, 0, 0x11, 0x0E, 0, 0, 0, 0}, // step 3601
        {1, 0, 1, 0, 0, 0, 0, 0},       // pause with steps
        {2, 1, 0, 0, 0, 0, 0, 0},       // reserved byte 9
        {1, 0, 0, 0, 0, 0, 0, 1},       // reserved byte 15
    };
    for (const auto& args : bad) {
        std::uint8_t bytes[16];
        raw(11, args, bytes);
        Message m;
        CHECK(decode(bytes, m) == Reject::Range);
    }
}

NNH_TEST(live_wire_types_past_the_last_are_unknown) {
    const std::uint8_t args[8] = {};
    std::uint8_t bytes[16];
    raw(18, args, bytes); // 12–17 are spec 031's reload messages
    Message m;
    CHECK(decode(bytes, m) == Reject::Type);
}

NNH_TEST(live_wire_record_lengths_at_bounds) {
    struct Case {
        RecordType type;
        std::uint32_t ok_low, ok_high;
    };
    const Case cases[] = {
        {RecordType::LiveEntry, kLiveEntryMin, kLiveEntryMax},  {RecordType::LiveRemoved, 8, 8},
        {RecordType::LiveValue, kLiveValueFixed, kLiveValueMax}, {RecordType::LiveRefused, 8, 8},
        {RecordType::LiveState, 8, 8},
    };
    for (const Case& c : cases) {
        std::uint8_t h[kRecordHeaderSize];
        RecordHeader out;
        encode_record_header(c.type, c.ok_low, h);
        CHECK(decode_record_header(h, out) && out.type == c.type && out.length == c.ok_low);
        encode_record_header(c.type, c.ok_high, h);
        CHECK(decode_record_header(h, out));
        encode_record_header(c.type, c.ok_low - 1, h);
        CHECK(!decode_record_header(h, out));
        encode_record_header(c.type, c.ok_high + 1, h);
        CHECK(!decode_record_header(h, out));
    }
    CHECK(kLiveEntryMin == 33 && kLiveEntryMax == 575 && kLiveValueFixed == 12 && kLiveValueMax == 44);
}

NNH_TEST(live_wire_live_entry_round_trip) {
    LiveEntryHeader h;
    h.frame = 100;
    h.hash = 0xCAFEF00Du;
    h.kind = live::Kind::Enum;
    h.flags = kEntryTunable | kEntryChanged;
    h.value = 2;
    h.def = 1;
    h.max = 2;
    const char* names[3] = {"circle", "square", "diamond"};
    std::uint8_t buf[kRecordHeaderSize + kLiveEntryMax];
    const std::size_t n = encode_live_entry(h, "spawn/shape", names, 3, buf);
    CHECK(n == kRecordHeaderSize + 32 + 11 + (1 + 6) + (1 + 6) + (1 + 7));
    RecordHeader rh;
    CHECK(decode_record_header(buf, rh) && rh.type == RecordType::LiveEntry && rh.length == n - kRecordHeaderSize);
    LiveEntryView v;
    CHECK(decode_live_entry(buf + kRecordHeaderSize, rh.length, v));
    CHECK(v.header.frame == 100 && v.header.hash == 0xCAFEF00Du && v.header.kind == live::Kind::Enum);
    CHECK(v.header.enum_count == 3 && v.header.name_length == 11);
    CHECK(std::memcmp(v.name, "spawn/shape", 11) == 0);
    char name[16];
    CHECK(live_entry_enum_name(v, 2, name) == 7 && std::strcmp(name, "diamond") == 0);
    CHECK(live_entry_enum_name(v, 3, name) == 0);
}

NNH_TEST(live_wire_live_entry_refuses_inconsistent_payloads) {
    LiveEntryHeader h;
    h.hash = 5;
    h.kind = live::Kind::Float;
    h.flags = kEntryTunable;
    std::uint8_t buf[kRecordHeaderSize + kLiveEntryMax];
    const std::size_t n = encode_live_entry(h, "player/jump", nullptr, 0, buf);
    const std::uint32_t len = static_cast<std::uint32_t>(n - kRecordHeaderSize);
    LiveEntryView v;
    CHECK(decode_live_entry(buf + kRecordHeaderSize, len, v));
    CHECK(!decode_live_entry(buf + kRecordHeaderSize, len - 1, v)); // the name cut
    std::uint8_t* p = buf + kRecordHeaderSize;
    p[10] = 0; // name length 0
    CHECK(!decode_live_entry(p, len, v));
    p[10] = 11;
    p[8] = 0; // kind None
    CHECK(!decode_live_entry(p, len, v));
    p[8] = static_cast<std::uint8_t>(live::Kind::Float);
    p[11] = 1; // an enum count on a float
    CHECK(!decode_live_entry(p, len, v));
    p[11] = 0;
    p[32 + 6] = '.'; // a bad character in the name
    CHECK(!decode_live_entry(p, len, v));
}

NNH_TEST(live_wire_live_value_round_trip) {
    LiveValueRecord r;
    r.frame = 1234;
    r.hash = 77;
    r.kind = live::Kind::Vec2;
    r.flags = kValuePaused;
    r.length = 8;
    const std::uint8_t xy[8] = {0, 0, 0x48, 0x41, 0, 0, 0x20, 0x42}; // (12.5, 40)
    std::memcpy(r.value, xy, 8);
    std::uint8_t buf[kRecordHeaderSize + kLiveValueMax];
    const std::size_t n = encode_live_value(r, buf);
    CHECK(n == kRecordHeaderSize + 12 + 8);
    LiveValueRecord back;
    CHECK(decode_live_value(buf + kRecordHeaderSize, static_cast<std::uint32_t>(n - kRecordHeaderSize), back));
    CHECK(back.frame == 1234 && back.hash == 77 && back.kind == live::Kind::Vec2 && back.length == 8);
    CHECK(back.flags == kValuePaused && std::memcmp(back.value, xy, 8) == 0);
    buf[kRecordHeaderSize + 10] = 4; // a Vec2 of 4 bytes
    CHECK(!decode_live_value(buf + kRecordHeaderSize, static_cast<std::uint32_t>(n - kRecordHeaderSize), back));
}

NNH_TEST(live_wire_live_value_text) {
    LiveValueRecord r;
    r.hash = 1;
    r.kind = live::Kind::Text;
    r.length = 4;
    std::memcpy(r.value, "idle", 4);
    std::uint8_t buf[kRecordHeaderSize + kLiveValueMax];
    const std::size_t n = encode_live_value(r, buf);
    LiveValueRecord back;
    CHECK(decode_live_value(buf + kRecordHeaderSize, static_cast<std::uint32_t>(n - kRecordHeaderSize), back));
    CHECK(back.length == 4 && std::memcmp(back.value, "idle", 4) == 0);
}

NNH_TEST(live_wire_small_records_bytes) {
    std::uint8_t buf[kRecordHeaderSize + 8];
    CHECK(encode_live_refused(0x0102, live::Refusal::OutOfRange, 0xAABBCCDDu, buf) == 16);
    const std::uint8_t refused[16] = {0x1E, 'N', 'R', 11, 8, 0, 0, 0, 0x02, 0x01, 3, 0, 0xDD, 0xCC, 0xBB, 0xAA};
    CHECK(std::memcmp(buf, refused, 16) == 0);
    LiveRefusedRecord rr;
    CHECK(decode_live_refused(buf + 8, 8, rr) && rr.seq == 0x0102 && rr.reason == live::Refusal::OutOfRange);
    CHECK(encode_live_state(500, true, kStateCatalogueComplete, 3, buf) == 16);
    const std::uint8_t state[16] = {0x1E, 'N', 'R', 12, 8, 0, 0, 0, 0xF4, 0x01, 0, 0, 1, 1, 3, 0};
    CHECK(std::memcmp(buf, state, 16) == 0);
    LiveStateRecord sr;
    CHECK(decode_live_state(buf + 8, 8, sr) && sr.frame == 500 && sr.paused && sr.steps == 3);
    CHECK(encode_live_removed(9, 0x01020304u, buf) == 16);
    const std::uint8_t removed[16] = {0x1E, 'N', 'R', 9, 8, 0, 0, 0, 9, 0, 0, 0, 4, 3, 2, 1};
    CHECK(std::memcmp(buf, removed, 16) == 0);
    std::uint8_t bad[8] = {0, 0, 8, 0, 1, 0, 0, 0}; // reason 8
    CHECK(!decode_live_refused(bad, 8, rr));
    std::uint8_t bad_state[8] = {0, 0, 0, 0, 2, 0, 0, 0}; // state 2
    CHECK(!decode_live_state(bad_state, 8, sr));
}
