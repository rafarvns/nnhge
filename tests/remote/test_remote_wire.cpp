// Console remote wire format (spec 013 FR-8, FR-1): the 16-byte messages and the record header.

#include <cstdint>
#include <cstring>

#include "remote/remote_wire.hpp"
#include "test.hpp"

using namespace nnh::detail::remote;

namespace {

// A valid message of each type, byte by byte (little-endian), seq 0x0201, frame 0x00000A0B.
struct Bytes {
    std::uint8_t b[kMessageSize];
};
Bytes hello() { return {{0x52, 1, 0x01, 0x02, 0x0B, 0x0A, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0}}; }
Bytes buttons(std::uint32_t press, std::uint32_t release) {
    Bytes m{{0x52, 2, 0x01, 0x02, 0x0B, 0x0A, 0, 0}};
    for (int i = 0; i < 4; ++i) {
        m.b[8 + i] = static_cast<std::uint8_t>(press >> (8 * i));
        m.b[12 + i] = static_cast<std::uint8_t>(release >> (8 * i));
    }
    return m;
}
Bytes stick(std::uint8_t which, std::int16_t x, std::int16_t y) {
    const auto ux = static_cast<std::uint16_t>(x), uy = static_cast<std::uint16_t>(y);
    return {{0x52, 3, 0x01, 0x02, 0x0B, 0x0A, 0, 0, which, 0, static_cast<std::uint8_t>(ux),
             static_cast<std::uint8_t>(ux >> 8), static_cast<std::uint8_t>(uy), static_cast<std::uint8_t>(uy >> 8),
             0, 0}};
}
Bytes touch(std::uint8_t down, std::uint16_t x, std::uint16_t y) {
    return {{0x52, 4, 0x01, 0x02, 0x0B, 0x0A, 0, 0, down, 0, static_cast<std::uint8_t>(x),
             static_cast<std::uint8_t>(x >> 8), static_cast<std::uint8_t>(y), static_cast<std::uint8_t>(y >> 8), 0, 0}};
}
Bytes screenshot(std::uint8_t screens) {
    return {{0x52, 5, 0x01, 0x02, 0x0B, 0x0A, 0, 0, 0x34, 0x12, screens, 0, 0, 0, 0, 0}};
}
Bytes bye(std::uint8_t quit) { return {{0x52, 6, 0x01, 0x02, 0x0B, 0x0A, 0, 0, quit, 0, 0, 0, 0, 0, 0, 0}}; }

Bytes language(const char* code) {
    Bytes m{{0x52, 8, 0x01, 0x02, 0x0B, 0x0A, 0, 0}};
    for (int i = 0; i < 8 && code[i] != 0; ++i) m.b[8 + i] = static_cast<std::uint8_t>(code[i]);
    return m;
}

Reject decode(const Bytes& m) {
    Message out;
    return decode_message(m.b, out);
}

bool round_trips(const Bytes& m) {
    Message out;
    if (decode_message(m.b, out) != Reject::None) return false;
    std::uint8_t again[kMessageSize];
    encode_message(out, again);
    return std::memcmp(again, m.b, kMessageSize) == 0;
}

} // namespace

NNH_TEST(remote_wire_decodes_the_header_fields) {
    Message out;
    CHECK(decode_message(hello().b, out) == Reject::None);
    CHECK(out.type == MessageType::Hello);
    CHECK(out.seq == 0x0201);
    CHECK(out.frame == 0x0A0B);
    CHECK(out.version == 1);
}

NNH_TEST(remote_wire_decodes_buttons_stick_touch_screenshot_and_bye) {
    Message out;
    CHECK(decode_message(buttons(0x0001, 0x2000).b, out) == Reject::None);
    CHECK(out.type == MessageType::Buttons && out.press == 0x0001 && out.release == 0x2000);
    CHECK(decode_message(stick(1, -1000, 250).b, out) == Reject::None);
    CHECK(out.type == MessageType::Stick && out.stick == 1 && out.x == -1000 && out.y == 250);
    CHECK(decode_message(touch(1, 319, 239).b, out) == Reject::None);
    CHECK(out.type == MessageType::Touch && out.down && out.x == 319 && out.y == 239);
    CHECK(decode_message(screenshot(3).b, out) == Reject::None);
    CHECK(out.type == MessageType::Screenshot && out.id == 0x1234 && out.screens == 3);
    CHECK(decode_message(bye(1).b, out) == Reject::None);
    CHECK(out.type == MessageType::Bye && out.quit);
}

NNH_TEST(remote_wire_messages_round_trip_byte_for_byte) {
    CHECK(round_trips(hello()));
    CHECK(round_trips(buttons(kButtonMask, 0)));
    CHECK(round_trips(buttons(0, kButtonMask)));
    CHECK(round_trips(stick(0, 1000, -1000)));
    CHECK(round_trips(touch(0, 0, 0)));
    CHECK(round_trips(touch(1, 160, 120)));
    CHECK(round_trips(screenshot(1)));
    CHECK(round_trips(screenshot(2)));
    CHECK(round_trips(bye(0)));
}

NNH_TEST(remote_wire_encodes_a_message_built_in_code) {
    Message m;
    m.type = MessageType::Stick;
    m.seq = 0x0201;
    m.frame = 0x0A0B;
    m.stick = 1;
    m.x = -1000;
    m.y = 250;
    std::uint8_t out[kMessageSize];
    encode_message(m, out);
    CHECK(std::memcmp(out, stick(1, -1000, 250).b, kMessageSize) == 0);
}

NNH_TEST(remote_wire_rejects_a_bad_magic) {
    Bytes m = hello();
    m.b[0] = 'r';
    CHECK(decode(m) == Reject::Magic);
}

NNH_TEST(remote_wire_rejects_unknown_types) {
    Bytes m = hello();
    m.b[1] = 0;
    CHECK(decode(m) == Reject::Type);
    m.b[1] = 9; // 7 is the 3D slider (spec 026), 8 the language (spec 029)
    CHECK(decode(m) == Reject::Type);
    m.b[1] = 0xFF;
    CHECK(decode(m) == Reject::Type);
}

NNH_TEST(remote_wire_rejects_a_hello_of_another_version_or_with_unknown_flags) {
    Bytes m = hello();
    m.b[8] = 2;
    CHECK(decode(m) == Reject::Range);
    m = hello();
    m.b[10] = 2;
    CHECK(decode(m) == Reject::Range);
    m = hello();
    m.b[10] = 3; // the known flag with an unknown one
    CHECK(decode(m) == Reject::Range);
    m = hello();
    m.b[11] = 0x80;
    CHECK(decode(m) == Reject::Range);
}

NNH_TEST(remote_wire_a_hello_without_flags_is_not_exclusive) {
    Message out;
    out.exclusive = true;
    CHECK(decode_message(hello().b, out) == Reject::None);
    CHECK(!out.exclusive);
}

NNH_TEST(remote_wire_the_exclusive_flag_is_bit_0_of_the_hello_flags) {
    Bytes m = hello();
    m.b[10] = 1;
    Message out;
    CHECK(decode_message(m.b, out) == Reject::None);
    CHECK(out.type == MessageType::Hello && out.exclusive);
    CHECK(round_trips(m));
    Message built;
    built.type = MessageType::Hello;
    built.seq = 0x0201;
    built.frame = 0x0A0B;
    built.version = kVersion;
    built.exclusive = true;
    std::uint8_t bytes[kMessageSize];
    encode_message(built, bytes);
    CHECK(std::memcmp(bytes, m.b, kMessageSize) == 0);
}

NNH_TEST(remote_wire_rejects_buttons_outside_the_mask) {
    CHECK(decode(buttons(kButtonMask + 1, 0)) == Reject::Range);
    CHECK(decode(buttons(0, 0x80000000u)) == Reject::Range);
}

NNH_TEST(remote_wire_rejects_overlapping_press_and_release_masks) {
    CHECK(decode(buttons(0x0003, 0x0002)) == Reject::Range);
    CHECK(decode(buttons(0x0001, 0x0002)) == Reject::None);
}

NNH_TEST(remote_wire_rejects_sticks_out_of_range) {
    CHECK(decode(stick(2, 0, 0)) == Reject::Range);
    CHECK(decode(stick(0, 1001, 0)) == Reject::Range);
    CHECK(decode(stick(0, -1001, 0)) == Reject::Range);
    CHECK(decode(stick(0, 0, 1001)) == Reject::Range);
    CHECK(decode(stick(0, 0, -1001)) == Reject::Range);
    CHECK(decode(stick(0, -1000, 1000)) == Reject::None);
}

NNH_TEST(remote_wire_rejects_touches_off_the_screen) {
    CHECK(decode(touch(2, 0, 0)) == Reject::Range);
    CHECK(decode(touch(1, 320, 0)) == Reject::Range);
    CHECK(decode(touch(1, 0, 240)) == Reject::Range);
    CHECK(decode(touch(1, 0xFFFF, 0)) == Reject::Range); // not a negative x
    CHECK(decode(touch(1, 319, 239)) == Reject::None);
}

NNH_TEST(remote_wire_rejects_screenshots_of_no_screen_or_unknown_screens) {
    CHECK(decode(screenshot(0)) == Reject::Range);
    CHECK(decode(screenshot(8)) == Reject::Range);
    CHECK(decode(screenshot(3)) == Reject::None);
    CHECK(decode(screenshot(7)) == Reject::None); // + the top screen's right eye (spec 026)
    CHECK(round_trips(screenshot(4)));
}

NNH_TEST(remote_wire_rejects_a_bye_with_quit_other_than_0_or_1) {
    CHECK(decode(bye(2)) == Reject::Range);
}

NNH_TEST(remote_wire_rejects_any_non_zero_reserved_byte) {
    // Every unused argument byte of every type, one at a time.
    struct Case {
        Bytes bytes;
        int first_unused;
    };
    const Case cases[] = {
        {hello(), 12}, {stick(0, 0, 0), 14}, {touch(0, 0, 0), 14}, {screenshot(1), 11}, {bye(0), 9},
    };
    for (const Case& c : cases) {
        for (int i = c.first_unused; i < static_cast<int>(kMessageSize); ++i) {
            Bytes m = c.bytes;
            m.b[i] = 1;
            CHECK(decode(m) == Reject::Range);
        }
    }
    Bytes s = stick(0, 0, 0); // the padding byte after `which`
    s.b[9] = 1;
    CHECK(decode(s) == Reject::Range);
    Bytes t = touch(0, 0, 0); // the padding byte after `down`
    t.b[9] = 1;
    CHECK(decode(t) == Reject::Range);
}

NNH_TEST(remote_wire_reads_the_seq_of_an_invalid_message) {
    Bytes m = hello();
    m.b[0] = 0;
    CHECK(message_seq(m.b) == 0x0201);
}

NNH_TEST(remote_wire_frame_0_means_the_frame_now_starting) {
    CHECK(resolve_frame(0, 41) == 41);
    CHECK(resolve_frame(7, 41) == 7);
    CHECK(resolve_frame(50, 41) == 50);
}

NNH_TEST(remote_wire_writes_and_reads_a_record_header) {
    std::uint8_t h[kRecordHeaderSize];
    encode_record_header(RecordType::Late, kLatePayload, h);
    const std::uint8_t expected[kRecordHeaderSize] = {0x1E, 'N', 'R', 3, 10, 0, 0, 0};
    CHECK(std::memcmp(h, expected, sizeof h) == 0);
    RecordHeader out;
    CHECK(decode_record_header(h, out));
    CHECK(out.type == RecordType::Late && out.length == kLatePayload);
}

NNH_TEST(remote_wire_record_header_needs_the_mark) {
    RecordHeader out;
    for (int i = 0; i < 3; ++i) {
        std::uint8_t h[kRecordHeaderSize];
        encode_record_header(RecordType::Tick, kTickPayload, h);
        h[i] ^= 0x40;
        CHECK(!decode_record_header(h, out));
    }
}

NNH_TEST(remote_wire_record_header_refuses_unknown_types) {
    RecordHeader out;
    std::uint8_t h[kRecordHeaderSize] = {0x1E, 'N', 'R', 0, 4, 0, 0, 0};
    CHECK(!decode_record_header(h, out));
    h[3] = 8;
    CHECK(!decode_record_header(h, out));
}

NNH_TEST(remote_wire_fixed_records_need_their_exact_length) {
    struct Case {
        RecordType type;
        std::uint32_t length;
    };
    const Case cases[] = {
        {RecordType::Hello, kHelloPayload},       {RecordType::Tick, kTickPayload},
        {RecordType::Late, kLatePayload},         {RecordType::Rejected, kRejectedPayload},
        {RecordType::Dropped, kDroppedPayload},   {RecordType::ScreenFailed, kScreenFailedPayload},
    };
    RecordHeader out;
    for (const Case& c : cases) {
        std::uint8_t h[kRecordHeaderSize];
        encode_record_header(c.type, c.length, h);
        CHECK(decode_record_header(h, out) && out.length == c.length);
        encode_record_header(c.type, c.length + 1, h);
        CHECK(!decode_record_header(h, out));
        encode_record_header(c.type, c.length - 1, h);
        CHECK(!decode_record_header(h, out));
    }
}

NNH_TEST(remote_wire_screen_records_are_capped) {
    RecordHeader out;
    std::uint8_t h[kRecordHeaderSize];
    encode_record_header(RecordType::Screen, kScreenFixed, h);
    CHECK(decode_record_header(h, out));
    encode_record_header(RecordType::Screen, kScreenFixed - 1, h);
    CHECK(!decode_record_header(h, out));
    encode_record_header(RecordType::Screen, kMaxRecordPayload, h);
    CHECK(decode_record_header(h, out) && out.length == kMaxRecordPayload);
    encode_record_header(RecordType::Screen, kMaxRecordPayload + 1, h);
    CHECK(!decode_record_header(h, out));
    encode_record_header(RecordType::Screen, 0xFFFFFFFFu, h);
    CHECK(!decode_record_header(h, out));
}

NNH_TEST(remote_wire_a_full_screen_fits_the_record_cap) {
    CHECK(kScreenFixed + 400u * 240u * 4u <= kMaxRecordPayload);
}

// Spec 029 FR-27: the Language message — bytes 8–15 the code, NUL-padded; all zero = off (the console's own language).
NNH_TEST(remote_wire_language_messages_round_trip) {
    CHECK(round_trips(language("ja")));
    CHECK(round_trips(language("pt-BR")));
    CHECK(round_trips(language("zh-Hant")));
    CHECK(round_trips(language("es-419")));
    CHECK(round_trips(language("")));
}

NNH_TEST(remote_wire_a_language_message_decodes_its_code) {
    Message out;
    CHECK(decode_message(language("pt-BR").b, out) == Reject::None);
    CHECK(out.type == MessageType::Language);
    char code[kLanguageCodeBytes + 1] = {};
    CHECK(unpack_language(out, code));
    CHECK(std::strcmp(code, "pt-BR") == 0);
    CHECK(decode_message(language("").b, out) == Reject::None);
    CHECK(!unpack_language(out, code)); // off
    CHECK(code[0] == 0);
}

NNH_TEST(remote_wire_a_language_message_built_in_code_encodes_its_bytes) {
    Message m;
    m.type = MessageType::Language;
    m.seq = 0x0201;
    m.frame = 0x0A0B;
    CHECK(pack_language("zh-Hant", m));
    std::uint8_t out[kMessageSize];
    encode_message(m, out);
    CHECK(std::memcmp(out, language("zh-Hant").b, kMessageSize) == 0);
    CHECK(pack_language(nullptr, m)); // off
    encode_message(m, out);
    CHECK(std::memcmp(out, language("").b, kMessageSize) == 0);
    CHECK(!pack_language("zh-Hans-CN", m)); // longer than 8 bytes
    CHECK(!pack_language("Japanese", m));
}

NNH_TEST(remote_wire_rejects_language_codes_of_the_wrong_shape) {
    // the decoder checks the shape; the full tag rules are pack_language's and system::language()'s
    CHECK(decode(language("j@")) == Reject::Range);
    CHECK(decode(language("-ja")) == Reject::Range);
    CHECK(decode(language("JA")) == Reject::Range);
    CHECK(decode(language("j")) == Reject::Range);
    CHECK(decode(language("ja ")) == Reject::Range);
    CHECK(decode(language("ja_JP")) == Reject::Range);
    Bytes interior = language("ja");
    interior.b[11] = 'x'; // "ja", NUL, then a byte: an interior NUL
    CHECK(decode(interior) == Reject::Range);
    Bytes high = language("ja");
    high.b[9] = 0xE3;
    CHECK(decode(high) == Reject::Range);
}
