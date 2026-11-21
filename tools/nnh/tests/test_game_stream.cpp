// The game's stream on the PC (remote/game_stream.hpp, spec 013 T3.1): log lines and records from arbitrary
// chunks, '\r' removed, text fallback for anything that isn't a record, caps. Records are built with core's
// own encoders (remote_wire.hpp), so both ends agree on the layouts.

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "remote/game_stream.hpp"
#include "test.hpp"

using namespace nnh::tool::remote;
namespace wire = nnh::detail::remote;

namespace {

using Bytes = std::vector<std::uint8_t>;

Bytes text(const char* s) { return Bytes(s, s + std::strlen(s)); }

Bytes operator+(Bytes a, const Bytes& b) {
    a.insert(a.end(), b.begin(), b.end());
    return a;
}

Bytes tick(std::uint32_t frame) {
    std::uint8_t out[wire::kMaxFixedRecord];
    return Bytes(out, out + wire::encode_tick_record(frame, out));
}

Bytes screen(std::uint16_t id, std::uint32_t pixel_bytes, std::uint8_t fill) {
    std::uint8_t out[wire::kMaxFixedRecord];
    const wire::ScreenHeader h{.id = id, .screen = 1, .layout = 1, .frame = 77, .width = 2, .height = 3};
    Bytes b(out, out + wire::encode_screen_record(h, pixel_bytes, out));
    b.insert(b.end(), pixel_bytes, fill);
    return b;
}

/// Feeds `bytes` in chunks of `chunk` bytes, then finishes the stream.
std::vector<StreamItem> split(const Bytes& bytes, std::size_t chunk) {
    GameStream stream;
    std::vector<StreamItem> items;
    for (std::size_t at = 0; at < bytes.size(); at += chunk) {
        const std::size_t n = bytes.size() - at < chunk ? bytes.size() - at : chunk;
        stream.feed(bytes.data() + at, n, items);
    }
    stream.finish(items);
    return items;
}

} // namespace

NNH_TEST(stream_splits_lines_in_any_chunks) {
    const Bytes in = text("[f1 0.0s] I app one\n[f2 0.0s] I app two\nthree");
    for (std::size_t chunk : {1u, 2u, 7u, 1000u}) {
        const auto items = split(in, chunk);
        CHECK(items.size() == 3);
        CHECK(items.size() == 3 && items[0].line == "[f1 0.0s] I app one" && items[1].line == "[f2 0.0s] I app two");
        CHECK(items.size() == 3 && items[2].line == "three" && !items[2].is_record);
    }
}

NNH_TEST(stream_removes_carriage_returns) {
    const auto items = split(text("a\r\nb\rc\r\n"), 3);
    CHECK(items.size() == 2 && items[0].line == "a" && items[1].line == "bc");
}

NNH_TEST(stream_keeps_empty_lines) {
    const auto items = split(text("\n\nx\n"), 1);
    CHECK(items.size() == 3 && items[0].line.empty() && items[1].line.empty() && items[2].line == "x");
}

NNH_TEST(stream_returns_records_between_lines_in_order) {
    const Bytes in = text("before\n") + tick(42) + text("after\n");
    for (std::size_t chunk : {1u, 3u, 8u, 9u, 4096u}) {
        const auto items = split(in, chunk);
        CHECK(items.size() == 3);
        if (items.size() != 3) continue;
        CHECK(items[0].line == "before" && !items[0].is_record);
        CHECK(items[1].is_record && items[1].record.type == wire::RecordType::Tick && items[1].record.frame == 42);
        CHECK(items[2].line == "after" && !items[2].is_record);
    }
}

NNH_TEST(stream_returns_a_partial_line_before_a_record) {
    const auto items = split(text("half a li") + tick(5) + text("ne\n"), 4);
    CHECK(items.size() == 3);
    CHECK(items.size() == 3 && items[0].line == "half a li" && items[1].is_record && items[2].line == "ne");
}

NNH_TEST(stream_decodes_every_record_type) {
    std::uint8_t out[wire::kMaxFixedRecord];
    Bytes in(out, out + wire::encode_hello_record(9, 0x30, "Development", out));
    in = in + Bytes(out, out + wire::encode_late_record(7, 100, 103, out));
    in = in + Bytes(out, out + wire::encode_rejected_record(8, wire::Reject::Order, out));
    in = in + Bytes(out, out + wire::encode_dropped_record(11, out));
    in = in + Bytes(out, out + wire::encode_screen_failed_record(3, 1, 55, out));
    const auto items = split(in, 5);
    CHECK(items.size() == 5);
    if (items.size() != 5) return;
    const Record& hello = items[0].record;
    CHECK(hello.type == wire::RecordType::Hello && hello.version == wire::kVersion && hello.flags == 0);
    CHECK(hello.frame == 9 && hello.capabilities == 0x30 && hello.flavor == "Development");
    CHECK(items[1].record.type == wire::RecordType::Late && items[1].record.seq == 7);
    CHECK(items[1].record.wanted == 100 && items[1].record.applied == 103);
    CHECK(items[2].record.type == wire::RecordType::Rejected && items[2].record.seq == 8);
    CHECK(items[2].record.reason == wire::Reject::Order);
    CHECK(items[3].record.type == wire::RecordType::Dropped && items[3].record.seq == 11);
    const Record& failed = items[4].record;
    CHECK(failed.type == wire::RecordType::ScreenFailed && failed.screen.id == 3 && failed.screen.screen == 1);
    CHECK(failed.frame == 55);
}

NNH_TEST(stream_keeps_a_flavor_of_the_full_12_characters) {
    std::uint8_t out[wire::kMaxFixedRecord];
    const auto items = split(Bytes(out, out + wire::encode_hello_record(1, 0, "ABCDEFGHIJKLMN", out)), 64);
    CHECK(items.size() == 1 && items[0].record.flavor == "ABCDEFGHIJKL");
}

NNH_TEST(stream_decodes_a_screen_record_with_its_pixels) {
    const auto items = split(screen(4, 18, 0xAB), 7);
    CHECK(items.size() == 1);
    if (items.size() != 1) return;
    const Record& r = items[0].record;
    CHECK(items[0].is_record && r.type == wire::RecordType::Screen);
    CHECK(r.screen.id == 4 && r.screen.screen == 1 && r.screen.layout == 1 && r.screen.frame == 77);
    CHECK(r.frame == 77 && r.screen.width == 2 && r.screen.height == 3);
    CHECK(r.pixels == Bytes(18, 0xAB));
}

NNH_TEST(stream_treats_a_bad_record_header_as_text) {
    Bytes bad = tick(1);
    bad[3] = 99; // unknown type
    const auto items = split(text("x") + bad + text("\n"), 2);
    CHECK(items.size() == 1 && !items[0].is_record);
    CHECK(items.size() == 1 && items[0].line.size() == 1 + bad.size() && items[0].line[1] == '\x1E');
}

NNH_TEST(stream_finds_a_record_right_after_a_false_start) {
    const Bytes in = Bytes{0x1E, 'N'} + tick(6) + text("z\n");
    const auto items = split(in, 3);
    CHECK(items.size() == 3);
    if (items.size() != 3) return;
    CHECK(items[0].line == "\x1EN" && items[1].is_record && items[1].record.frame == 6 && items[2].line == "z");
}

NNH_TEST(stream_treats_a_record_over_the_cap_as_text) {
    std::uint8_t header[wire::kRecordHeaderSize];
    wire::encode_record_header(wire::RecordType::Screen, wire::kMaxRecordPayload + 1, header);
    const auto items = split(Bytes(header, header + 8) + text("\n"), 8);
    CHECK(items.size() == 1 && !items[0].is_record && items[0].line.size() == 8);
}

NNH_TEST(stream_shows_an_unfinished_record_as_text_at_the_end) {
    Bytes cut = screen(1, 18, 'p');
    cut.resize(cut.size() - 5);
    const auto items = split(text("a\n") + cut, 4);
    CHECK(items.size() == 2 && items[0].line == "a");
    CHECK(items.size() == 2 && !items[1].is_record && items[1].line.size() == cut.size());
}

NNH_TEST(stream_cuts_a_line_at_the_cap) {
    Bytes in(kMaxLine + 4, 'q');
    in.back() = '\n'; // (not push_back: GCC 16 -O2 warns falsely about the reallocation)
    const auto items = split(in, 65536);
    CHECK(items.size() == 2 && items[0].line.size() == kMaxLine && items[1].line == "qqq");
}
