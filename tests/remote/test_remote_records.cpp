// Console remote records (spec 013 FR-1, FR-2, FR-8): the console → PC records the session writes, byte by byte.

#include <cstdint>
#include <cstring>

#include "remote/remote_wire.hpp"
#include "test.hpp"

using namespace nnh::detail::remote;

namespace {

bool bytes_are(const std::uint8_t* got, std::size_t size, const std::uint8_t* want, std::size_t want_size) {
    return size == want_size && std::memcmp(got, want, size) == 0;
}

} // namespace

NNH_TEST(remote_records_hello_carries_version_frame_capabilities_and_flavor) {
    std::uint8_t out[kMaxFixedRecord];
    const std::size_t n = encode_hello_record(0x01020304u, 0x0000002Au, "Development", out);
    const std::uint8_t want[] = {0x1E, 'N',  'R',  1,    24,   0,   0,   0,   // header
                                 1,    0,    0,    0,                         // version 1, flags 0
                                 0x04, 0x03, 0x02, 0x01,                      // frame
                                 0x2A, 0,    0,    0,                         // capabilities
                                 'D',  'e',  'v',  'e',  'l', 'o', 'p', 'm', 'e', 'n', 't', 0};
    CHECK(bytes_are(out, n, want, sizeof want));
}

NNH_TEST(remote_records_hello_pads_a_short_flavor_and_cuts_a_long_one) {
    std::uint8_t out[kMaxFixedRecord];
    std::memset(out, 0xEE, sizeof out);
    CHECK(encode_hello_record(0, 0, "Debug", out) == kRecordHeaderSize + kHelloPayload);
    const std::uint8_t debug[12] = {'D', 'e', 'b', 'u', 'g', 0, 0, 0, 0, 0, 0, 0};
    CHECK(std::memcmp(out + 20, debug, 12) == 0);
    encode_hello_record(0, 0, "ABCDEFGHIJKLMNOP", out);
    CHECK(std::memcmp(out + 20, "ABCDEFGHIJKL", 12) == 0);
}

NNH_TEST(remote_records_tick_carries_its_frame) {
    std::uint8_t out[kMaxFixedRecord];
    const std::size_t n = encode_tick_record(0x00ABCDEFu, out);
    const std::uint8_t want[] = {0x1E, 'N', 'R', 2, 4, 0, 0, 0, 0xEF, 0xCD, 0xAB, 0};
    CHECK(bytes_are(out, n, want, sizeof want));
}

NNH_TEST(remote_records_late_carries_seq_wanted_and_applied_frames) {
    std::uint8_t out[kMaxFixedRecord];
    const std::size_t n = encode_late_record(0x0102, 10, 0x0100000Cu, out);
    const std::uint8_t want[] = {0x1E, 'N', 'R', 3, 10, 0, 0, 0, 0x02, 0x01, 10, 0, 0, 0, 0x0C, 0, 0, 0x01};
    CHECK(bytes_are(out, n, want, sizeof want));
}

NNH_TEST(remote_records_rejected_carries_seq_and_reason) {
    std::uint8_t out[kMaxFixedRecord];
    const std::size_t n = encode_rejected_record(0xBEEF, Reject::Order, out);
    const std::uint8_t want[] = {0x1E, 'N', 'R', 4, 3, 0, 0, 0, 0xEF, 0xBE, 4};
    CHECK(bytes_are(out, n, want, sizeof want));
}

NNH_TEST(remote_records_dropped_carries_seq) {
    std::uint8_t out[kMaxFixedRecord];
    const std::size_t n = encode_dropped_record(0x0304, out);
    const std::uint8_t want[] = {0x1E, 'N', 'R', 5, 2, 0, 0, 0, 0x04, 0x03};
    CHECK(bytes_are(out, n, want, sizeof want));
}

NNH_TEST(remote_records_screen_header_counts_the_pixels_that_follow) {
    std::uint8_t out[kMaxFixedRecord];
    ScreenHeader h;
    h.id = 0x0201;
    h.screen = 1;
    h.layout = 2;
    h.frame = 0x00010203u;
    h.width = 320;
    h.height = 240;
    const std::size_t n = encode_screen_record(h, 320u * 240u * 3u, out);
    const std::uint32_t length = kScreenFixed + 320u * 240u * 3u; // 0x0003_840C
    const std::uint8_t want[] = {0x1E,
                                 'N',
                                 'R',
                                 6,
                                 static_cast<std::uint8_t>(length),
                                 static_cast<std::uint8_t>(length >> 8),
                                 static_cast<std::uint8_t>(length >> 16),
                                 0,
                                 0x01, 0x02, 1, 2,       // id, screen, layout
                                 0x03, 0x02, 0x01, 0x00, // frame
                                 0x40, 0x01, 0xF0, 0x00}; // 320, 240
    CHECK(bytes_are(out, n, want, sizeof want));
    RecordHeader header;
    CHECK(decode_record_header(out, header) && header.type == RecordType::Screen && header.length == length);
}

NNH_TEST(remote_records_screen_failed_carries_id_screen_and_frame) {
    std::uint8_t out[kMaxFixedRecord];
    const std::size_t n = encode_screen_failed_record(0x0A0B, 0, 77, out);
    const std::uint8_t want[] = {0x1E, 'N', 'R', 7, 7, 0, 0, 0, 0x0B, 0x0A, 0, 77, 0, 0, 0};
    CHECK(bytes_are(out, n, want, sizeof want));
}

NNH_TEST(remote_records_every_fixed_record_passes_the_header_check) {
    std::uint8_t out[kMaxFixedRecord];
    RecordHeader h;
    std::size_t n = encode_hello_record(1, 2, "Debug", out);
    CHECK(decode_record_header(out, h) && n == kRecordHeaderSize + h.length);
    n = encode_tick_record(1, out);
    CHECK(decode_record_header(out, h) && n == kRecordHeaderSize + h.length);
    n = encode_late_record(1, 2, 3, out);
    CHECK(decode_record_header(out, h) && n == kRecordHeaderSize + h.length);
    n = encode_rejected_record(1, Reject::Magic, out);
    CHECK(decode_record_header(out, h) && n == kRecordHeaderSize + h.length);
    n = encode_dropped_record(1, out);
    CHECK(decode_record_header(out, h) && n == kRecordHeaderSize + h.length);
    n = encode_screen_failed_record(1, 1, 2, out);
    CHECK(decode_record_header(out, h) && n == kRecordHeaderSize + h.length);
}
