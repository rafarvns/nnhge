// The reader's payload mode (spec 031 T1.2, D54, FR-18): after a message that announces raw bytes, those bytes come out
// as pieces pointing into the caller's buffer, split across reads, and decoding resumes after them. Transfer messages
// skip the frame order (they are handled at receipt); a refused one still skips its payload; malformed headers never
// announce one, so 8 in a row still switch the reader off.

#include <cstdint>
#include <cstring>

#include "reload/reload_wire.hpp"
#include "remote/remote_reader.hpp"
#include "test.hpp"

using namespace nnh::detail::remote;

namespace {

constexpr std::uint32_t kNow = 100;

struct Wire {
    std::uint8_t bytes[kMessageSize * 8 + 16384 + 64] = {};
    std::size_t size = 0;

    Wire& add(const Message& m) {
        encode_message(m, bytes + size);
        size += kMessageSize;
        return *this;
    }
    Wire& raw(const void* data, std::size_t n) {
        std::memcpy(bytes + size, data, n);
        size += n;
        return *this;
    }
    Wire& fill(std::uint8_t value, std::size_t n) {
        std::memset(bytes + size, value, n);
        size += n;
        return *this;
    }
};

Message hello() {
    Message m;
    m.type = MessageType::Hello;
    m.version = kVersion;
    return m;
}

Message press(std::uint32_t frame) {
    Message m;
    m.type = MessageType::Buttons;
    m.frame = frame;
    m.press = 1;
    return m;
}

/// Everything a feed produced, payload bytes joined.
struct Collected {
    int messages = 0;
    int pieces = 0;
    int ends = 0;
    MessageType types[16] = {};
    Reject reasons[16] = {};
    std::uint8_t payload[20000] = {};
    std::size_t payload_size = 0;
};

void feed_all(RemoteReader& r, const std::uint8_t* data, std::size_t size, Collected& c) {
    ReadItem items[8];
    std::size_t at = 0;
    while (at < size) {
        const FeedResult fed = r.feed(data + at, size - at, kNow, items, 8);
        for (std::size_t i = 0; i < fed.items; ++i) {
            if (items[i].payload_size > 0) {
                std::memcpy(c.payload + c.payload_size, items[i].payload, items[i].payload_size);
                c.payload_size += items[i].payload_size;
                ++c.pieces;
                if (items[i].payload_end) ++c.ends;
            } else {
                c.types[c.messages] = items[i].message.type;
                c.reasons[c.messages] = items[i].reason;
                ++c.messages;
            }
        }
        at += fed.consumed;
        if (fed.consumed == 0 && fed.items == 0) break;
    }
}

} // namespace

NNH_TEST(remote_reader_payload_follows_a_file_message) {
    Wire w;
    w.add(hello()).add(reload_file_message(0, ReloadOp::Put, 3, 10)).raw("a/b", 3).raw("\x01\x02\x03\x04", 4).add(press(0));
    RemoteReader r;
    Collected c;
    feed_all(r, w.bytes, w.size, c);
    CHECK(c.messages == 3);
    CHECK(c.types[1] == MessageType::ReloadFile);
    CHECK(c.types[2] == MessageType::Buttons);
    CHECK(c.reasons[2] == Reject::None);
    CHECK(c.payload_size == 7);
    CHECK(c.ends == 1);
    CHECK(std::memcmp(c.payload, "a/b\x01\x02\x03\x04", 7) == 0);
}

NNH_TEST(remote_reader_payload_split_at_every_point) {
    Wire w;
    w.add(hello()).add(reload_chunk_message(0, 40, 0));
    for (int i = 0; i < 40; ++i) {
        const std::uint8_t b = static_cast<std::uint8_t>(i + 1);
        w.raw(&b, 1);
    }
    w.add(press(0));
    for (std::size_t split = 1; split < w.size; ++split) {
        RemoteReader r;
        Collected c;
        feed_all(r, w.bytes, split, c);
        feed_all(r, w.bytes + split, w.size - split, c);
        CHECK(c.messages == 3);
        CHECK(c.payload_size == 40);
        CHECK(c.ends == 1);
        CHECK(c.payload[0] == 1 && c.payload[39] == 40);
        CHECK(c.reasons[2] == Reject::None);
    }
}

NNH_TEST(remote_reader_payload_of_one_and_of_16384_bytes) {
    Wire w;
    w.add(hello()).add(reload_chunk_message(0, 1, 0)).fill(7, 1).add(reload_chunk_message(0, 16384, 1)).fill(9, 16384);
    w.add(press(0));
    RemoteReader r;
    Collected c;
    feed_all(r, w.bytes, w.size, c);
    CHECK(c.messages == 4);
    CHECK(c.payload_size == 16385);
    CHECK(c.ends == 2);
    CHECK(c.payload[0] == 7 && c.payload[16384] == 9);
}

NNH_TEST(remote_reader_bytes_wanted_in_payload_mode_is_what_is_left) {
    Wire w;
    w.add(hello()).add(reload_chunk_message(0, 100, 0)).fill(1, 30);
    RemoteReader r;
    Collected c;
    feed_all(r, w.bytes, w.size, c);
    CHECK(r.payload_left() == 70);
    CHECK(r.bytes_wanted(4) == 70);
    CHECK(r.bytes_wanted(0) == 0); // a piece needs an output slot too
}

NNH_TEST(remote_reader_a_piece_needs_an_output_slot) {
    Wire w;
    w.add(hello()).add(reload_chunk_message(0, 10, 0)).fill(1, 10);
    RemoteReader r;
    ReadItem items[2];
    const FeedResult first = r.feed(w.bytes, w.size, kNow, items, 2); // Hello + Chunk fill the slots
    CHECK(first.items == 2);
    CHECK(first.consumed == 32);
    const FeedResult second = r.feed(w.bytes + 32, w.size - 32, kNow, items, 2);
    CHECK(second.items == 1);
    CHECK(items[0].payload_size == 10);
    CHECK(items[0].payload_end);
}

NNH_TEST(remote_reader_a_refused_transfer_message_skips_its_payload) {
    Wire w; // no Hello: NotAttached, but the stream stays in sync
    w.add(reload_chunk_message(0, 20, 0)).fill(0xAB, 20).add(hello()).add(press(0));
    RemoteReader r;
    Collected c;
    feed_all(r, w.bytes, w.size, c);
    CHECK(c.messages == 3);
    CHECK(c.reasons[0] == Reject::NotAttached);
    CHECK(c.payload_size == 0);
    CHECK(c.reasons[1] == Reject::None);
    CHECK(c.reasons[2] == Reject::None);
}

NNH_TEST(remote_reader_transfer_messages_skip_the_frame_order) {
    Wire w;
    w.add(hello()).add(press(500)).add(reload_begin_message(1, 1, 5)).add(reload_cancel_message(1)).add(press(400));
    RemoteReader r;
    Collected c;
    feed_all(r, w.bytes, w.size, c);
    CHECK(c.messages == 5);
    CHECK(c.reasons[2] == Reject::None); // resolved to now (100) < 500, still accepted
    CHECK(c.reasons[3] == Reject::None);
    CHECK(c.reasons[4] == Reject::Order); // the order of scheduled messages is unchanged
}

NNH_TEST(remote_reader_apply_keeps_the_frame_order) {
    Wire w;
    w.add(hello()).add(press(500)).add(reload_apply_message(1, 1, 400));
    RemoteReader r;
    Collected c;
    feed_all(r, w.bytes, w.size, c);
    CHECK(c.reasons[2] == Reject::Order);
}

NNH_TEST(remote_reader_malformed_headers_never_announce_a_payload) {
    Wire w;
    w.add(hello());
    for (int i = 0; i < 8; ++i) {
        Message m = reload_chunk_message(0, 16, 0);
        std::uint8_t bytes[16];
        encode_message(m, bytes);
        bytes[15] = 0xFF; // offset past 16 MiB: Range, its length isn't trusted
        w.raw(bytes, 16);
    }
    RemoteReader r;
    Collected c;
    feed_all(r, w.bytes, w.size, c);
    CHECK(c.messages == 9);
    CHECK(c.payload_size == 0);
    CHECK(r.off());
}

NNH_TEST(remote_reader_reset_forgets_a_payload) {
    Wire w;
    w.add(hello()).add(reload_chunk_message(0, 100, 0)).fill(1, 10);
    RemoteReader r;
    Collected c;
    feed_all(r, w.bytes, w.size, c);
    CHECK(r.payload_left() == 90);
    r.reset();
    CHECK(r.payload_left() == 0);
}
