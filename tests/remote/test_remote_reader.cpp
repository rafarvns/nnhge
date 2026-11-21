// Console remote reader (spec 013 FR-8): bytes → messages, Hello first, order, malformed counting, bounded.

#include <cstdint>
#include <cstring>

#include "remote/remote_reader.hpp"
#include "test.hpp"

using namespace nnh::detail::remote;

namespace {

constexpr std::uint32_t kNow = 100;

struct Wire {
    std::uint8_t bytes[kMessageSize * 16] = {};
    std::size_t size = 0;

    Wire& add(const Message& m) {
        encode_message(m, bytes + size);
        size += kMessageSize;
        return *this;
    }
    Wire& add_garbage() {
        for (std::size_t i = 0; i < kMessageSize; ++i) bytes[size + i] = 0xAB;
        size += kMessageSize;
        return *this;
    }
};

Message hello(std::uint16_t seq = 1, std::uint32_t frame = 0) {
    Message m;
    m.type = MessageType::Hello;
    m.seq = seq;
    m.frame = frame;
    m.version = kVersion;
    return m;
}

Message press(std::uint16_t seq, std::uint32_t frame, std::uint32_t mask = 1) {
    Message m;
    m.type = MessageType::Buttons;
    m.seq = seq;
    m.frame = frame;
    m.press = mask;
    return m;
}

Message bye(std::uint16_t seq) {
    Message m;
    m.type = MessageType::Bye;
    m.seq = seq;
    return m;
}

struct Fed {
    ReadItem items[16];
    FeedResult result;
};

Fed feed(RemoteReader& reader, const Wire& wire, std::uint32_t now = kNow) {
    Fed f;
    f.result = reader.feed(wire.bytes, wire.size, now, f.items, 16);
    return f;
}

RemoteReader attached_reader() {
    RemoteReader reader;
    feed(reader, Wire{}.add(hello()));
    return reader;
}

} // namespace

NNH_TEST(remote_reader_decodes_a_whole_message) {
    RemoteReader reader;
    const Fed f = feed(reader, Wire{}.add(hello(7)));
    CHECK(f.result.items == 1);
    CHECK(f.result.consumed == kMessageSize);
    CHECK(f.items[0].reason == Reject::None);
    CHECK(f.items[0].message.type == MessageType::Hello && f.items[0].message.seq == 7);
}

NNH_TEST(remote_reader_joins_a_message_split_across_reads) {
    RemoteReader reader;
    Wire wire;
    wire.add(hello(3));
    ReadItem items[4];
    for (std::size_t i = 0; i + 1 < kMessageSize; ++i) {
        CHECK(reader.feed(wire.bytes + i, 1, kNow, items, 4).items == 0);
    }
    const FeedResult last = reader.feed(wire.bytes + kMessageSize - 1, 1, kNow, items, 4);
    CHECK(last.items == 1);
    CHECK(items[0].message.seq == 3);
    CHECK(reader.attached());
}

NNH_TEST(remote_reader_reads_several_messages_from_one_read) {
    RemoteReader reader;
    Wire wire;
    wire.add(hello()).add(press(2, 101)).add(press(3, 102));
    // Split in the middle of the second message: the rest completes it.
    ReadItem items[4];
    const FeedResult first = reader.feed(wire.bytes, 20, kNow, items, 4);
    CHECK(first.items == 1 && first.consumed == 20);
    const FeedResult second = reader.feed(wire.bytes + 20, wire.size - 20, kNow, items, 4);
    CHECK(second.items == 2);
    CHECK(items[0].message.seq == 2 && items[1].message.seq == 3);
    CHECK(items[0].reason == Reject::None && items[1].reason == Reject::None);
}

NNH_TEST(remote_reader_requires_hello_first) {
    RemoteReader reader;
    const Fed f = feed(reader, Wire{}.add(press(9, 101)).add(hello()).add(press(10, 101)));
    CHECK(f.result.items == 3);
    CHECK(f.items[0].reason == Reject::NotAttached && f.items[0].message.seq == 9);
    CHECK(f.items[1].reason == Reject::None);
    CHECK(f.items[2].reason == Reject::None);
}

NNH_TEST(remote_reader_passes_decode_errors_on_with_the_seq) {
    RemoteReader reader = attached_reader();
    Message bad = press(42, 101);
    bad.press = 1u << 20;
    Wire wire;
    wire.add(press(1, 101));
    std::uint8_t raw[kMessageSize];
    encode_message(bad, raw);
    std::memcpy(wire.bytes + wire.size, raw, kMessageSize);
    wire.size += kMessageSize;
    const Fed f = feed(reader, wire);
    CHECK(f.items[1].reason == Reject::Range);
    CHECK(f.items[1].message.seq == 42);
}

NNH_TEST(remote_reader_resolves_frame_0_to_the_frame_now_starting) {
    RemoteReader reader = attached_reader();
    const Fed f = feed(reader, Wire{}.add(press(2, 0)), 77);
    CHECK(f.items[0].reason == Reject::None);
    CHECK(f.items[0].message.frame == 77);
}

NNH_TEST(remote_reader_keeps_frames_in_order) {
    RemoteReader reader = attached_reader();
    const Fed f = feed(reader, Wire{}.add(press(2, 110)).add(press(3, 110)).add(press(4, 109)).add(press(5, 111)));
    CHECK(f.items[0].reason == Reject::None);
    CHECK(f.items[1].reason == Reject::None); // same frame is fine
    CHECK(f.items[2].reason == Reject::Order);
    CHECK(f.items[3].reason == Reject::None);
}

NNH_TEST(remote_reader_orders_frame_0_by_its_resolved_frame) {
    RemoteReader reader = attached_reader();
    const Fed f = feed(reader, Wire{}.add(press(2, 150)).add(press(3, 0)), 120);
    CHECK(f.items[1].reason == Reject::Order); // "now" (120) comes before 150
}

NNH_TEST(remote_reader_rejected_messages_do_not_move_the_order) {
    RemoteReader reader = attached_reader();
    Message bad = press(2, 500);
    bad.release = 1; // overlaps press: Range
    const Fed f = feed(reader, Wire{}.add(bad).add(press(3, 120)));
    CHECK(f.items[0].reason == Reject::Range);
    CHECK(f.items[1].reason == Reject::None);
}

NNH_TEST(remote_reader_does_not_order_hello) {
    RemoteReader reader = attached_reader();
    const Fed f = feed(reader, Wire{}.add(press(2, 130)).add(hello(3, 5)).add(press(4, 130)));
    CHECK(f.items[1].reason == Reject::None);
    CHECK(f.items[2].reason == Reject::None);
}

NNH_TEST(remote_reader_bye_detaches) {
    RemoteReader reader = attached_reader();
    const Fed f = feed(reader, Wire{}.add(bye(2)).add(press(3, 120)));
    CHECK(f.items[0].reason == Reject::None && f.items[0].message.type == MessageType::Bye);
    CHECK(f.items[1].reason == Reject::NotAttached);
    CHECK(!reader.attached());
}

NNH_TEST(remote_reader_turns_off_after_8_bad_messages_in_a_row) {
    RemoteReader reader = attached_reader();
    Wire wire;
    for (int i = 0; i < kMaxBadInARow - 1; ++i) wire.add_garbage();
    feed(reader, wire);
    CHECK(!reader.off());
    feed(reader, Wire{}.add_garbage());
    CHECK(reader.off());
    const Fed f = feed(reader, Wire{}.add(hello()).add(press(2, 120)));
    CHECK(f.items[0].reason == Reject::TooMany);
    CHECK(f.items[1].reason == Reject::TooMany);
}

NNH_TEST(remote_reader_a_good_message_resets_the_bad_count) {
    RemoteReader reader = attached_reader();
    Wire wire;
    for (int i = 0; i < kMaxBadInARow - 1; ++i) wire.add_garbage();
    wire.add(press(2, 120));
    for (int i = 0; i < kMaxBadInARow - 1; ++i) wire.add_garbage();
    feed(reader, wire);
    CHECK(!reader.off());
}

NNH_TEST(remote_reader_messages_before_hello_count_as_bad) {
    RemoteReader reader;
    Wire wire;
    for (int i = 0; i < kMaxBadInARow; ++i) wire.add(press(static_cast<std::uint16_t>(i), 101));
    feed(reader, wire);
    CHECK(reader.off());
}

NNH_TEST(remote_reader_reset_starts_over) {
    RemoteReader reader = attached_reader();
    Wire wire;
    for (int i = 0; i < kMaxBadInARow; ++i) wire.add_garbage();
    feed(reader, wire);
    reader.feed(wire.bytes, 5, kNow, nullptr, 0); // a partial message left behind
    reader.reset();
    CHECK(!reader.off() && !reader.attached());
    const Fed f = feed(reader, Wire{}.add(hello()).add(press(2, 1)));
    CHECK(f.result.items == 2);
    CHECK(f.items[0].reason == Reject::None && f.items[1].reason == Reject::None);
}

NNH_TEST(remote_reader_asks_only_for_the_room_given) {
    RemoteReader reader;
    CHECK(reader.bytes_wanted(0) == 0);
    CHECK(reader.bytes_wanted(1) == kMessageSize);
    CHECK(reader.bytes_wanted(64) == 64 * kMessageSize);
    Wire wire;
    wire.add(hello());
    ReadItem items[1];
    reader.feed(wire.bytes, 5, kNow, items, 1);
    CHECK(reader.bytes_wanted(1) == kMessageSize - 5);
    CHECK(reader.bytes_wanted(2) == 2 * kMessageSize - 5);
    CHECK(reader.bytes_wanted(0) == 0);
}

NNH_TEST(remote_reader_stops_when_the_output_is_full) {
    RemoteReader reader;
    Wire wire;
    wire.add(hello()).add(press(2, 101)).add(press(3, 102));
    ReadItem items[2];
    const FeedResult r = reader.feed(wire.bytes, wire.size, kNow, items, 2);
    CHECK(r.items == 2);
    CHECK(r.consumed == 3 * kMessageSize - 1); // stops before the byte that would complete the third
    const FeedResult rest = reader.feed(wire.bytes + r.consumed, wire.size - r.consumed, kNow, items, 2);
    CHECK(rest.items == 1 && items[0].message.seq == 3);
}

NNH_TEST(remote_reader_with_no_room_takes_no_bytes) {
    RemoteReader reader;
    Wire wire;
    wire.add(hello());
    const FeedResult r = reader.feed(wire.bytes, wire.size, kNow, nullptr, 0);
    CHECK(r.items == 0 && r.consumed == kMessageSize - 1); // up to, not including, the completing byte
    CHECK(!reader.attached());
}
