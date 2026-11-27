// Fuzz target of the reload receiver (spec 031 T1.6, FR-18, AC-6): bytes → the remote reader (payload mode) → the
// receiver → a checking sink. Two modes by the first byte's low bit: (0) a raw stream, each chunk led by a control byte
// (bits 0-5: bytes that arrive, + 1); (1) well-formed but wrong sequences: every 6 bytes build one transfer message with
// small fields (and its payload), so order, offsets, sizes, totals and CRCs go wrong in every way. Invariants (abort on
// a break): nothing is appended outside an open put, a put never gets more bytes than announced, finish only after
// every byte, Cancel leaves the receiver idle.

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "reload/reload_receiver.hpp"
#include "reload/reload_wire.hpp"
#include "remote/remote_reader.hpp"

namespace nnh::fuzz {
namespace {

using namespace detail::remote;
using detail::reload::ReceiverEvent;
using detail::reload::ReceiverSink;
using detail::reload::ReloadReceiver;

struct Checker {
    bool open = false;
    std::uint32_t size = 0, got = 0;
};

void require(bool ok) {
    if (!ok) std::abort();
}

bool c_begin(void* c, std::uint16_t, const char* path, ReloadOp op, std::uint32_t size) {
    auto* k = static_cast<Checker*>(c);
    require(!k->open && path != nullptr && std::strlen(path) <= 255);
    k->open = op == ReloadOp::Put && size > 0;
    k->size = size;
    k->got = 0;
    return true;
}
bool c_append(void* c, const std::uint8_t*, std::size_t n) {
    auto* k = static_cast<Checker*>(c);
    require(k->open && n > 0 && k->got + n <= k->size);
    k->got += static_cast<std::uint32_t>(n);
    return true;
}
bool c_finish(void* c, std::uint16_t) {
    auto* k = static_cast<Checker*>(c);
    require(k->got == k->size);
    k->open = false;
    return true;
}
void c_discard(void* c) { *static_cast<Checker*>(c) = Checker{}; }

struct Run {
    Checker checker;
    ReceiverSink sink{&checker, c_begin, c_append, c_finish, c_discard};
    RemoteReader reader;
    ReloadReceiver receiver;
    std::uint32_t frame = 1;

    void feed(const std::uint8_t* data, std::size_t size) {
        ReadItem items[8];
        std::size_t at = 0;
        while (at < size) {
            const FeedResult fed = reader.feed(data + at, size - at, frame, items, 8);
            for (std::size_t i = 0; i < fed.items; ++i) handle(items[i]);
            at += fed.consumed;
        }
    }

    void handle(const ReadItem& item) {
        if (item.payload_size > 0) {
            receiver.on_payload(item.payload, item.payload_size, sink);
            return;
        }
        if (item.reason != Reject::None) return;
        const Message& m = item.message;
        if (m.type == MessageType::ReloadApply) {
            receiver.take(reload_batch(m), reload_files(m));
        } else if (is_reload_transfer(m.type) && m.type != MessageType::ReloadNotice) {
            const ReceiverEvent e = receiver.on_message(m, sink);
            if (e == ReceiverEvent::Cancelled) require(receiver.state() == ReloadReceiver::State::Idle);
        }
    }
};

void raw_mode(Run& run, const std::uint8_t* data, std::size_t size) {
    std::size_t at = 0;
    while (at < size) {
        const std::size_t n = (data[at] & 0x3Fu) + 1u;
        ++at;
        const std::size_t take = n < size - at ? n : size - at;
        run.feed(data + at, take);
        at += take;
        ++run.frame;
    }
}

void structured_mode(Run& run, const std::uint8_t* data, std::size_t size) {
    std::uint8_t out[kMessageSize + 64];
    Message hello;
    hello.type = MessageType::Hello;
    hello.version = kVersion;
    encode_message(hello, out);
    run.feed(out, kMessageSize);
    for (std::size_t at = 0; at + 6 <= size; at += 6) {
        const std::uint8_t* d = data + at;
        const std::uint16_t small = static_cast<std::uint16_t>(d[1] % 4);
        Message m;
        std::size_t extra = 0;
        switch (d[0] % 6) {
        case 0: m = reload_begin_message(static_cast<std::uint16_t>(1 + small), static_cast<std::uint16_t>(1 + d[2] % 3),
                                         d[3] % 64u); break;
        case 1: {
            const auto op = d[2] % 2 == 0 ? ReloadOp::Put : ReloadOp::Remove;
            const auto len = static_cast<std::uint8_t>(1 + d[3] % 6);
            m = reload_file_message(small, op, len, op == ReloadOp::Put ? d[4] % 16u : 0u);
            for (int i = 0; i < len; ++i) out[kMessageSize + i] = static_cast<std::uint8_t>('a' + (d[5] + i) % 3);
            if (d[5] % 7 == 0) out[kMessageSize] = '/'; // sometimes a bad path
            extra = payload_length(m);
            for (std::size_t i = len; i < extra; ++i) out[kMessageSize + i] = d[(i + 2) % 6]; // the CRC: mostly wrong
            break;
        }
        case 2: {
            const auto len = static_cast<std::uint16_t>(1 + d[2] % 16);
            m = reload_chunk_message(small, len, d[3] % 16u);
            for (int i = 0; i < len; ++i) out[kMessageSize + i] = d[(i + 4) % 6];
            extra = len;
            break;
        }
        case 3: m = reload_apply_message(static_cast<std::uint16_t>(1 + small), static_cast<std::uint16_t>(1 + d[2] % 3),
                                         0); break;
        case 4: m = reload_cancel_message(small); break;
        default: m = reload_notice_message(NoticeReason::ConversionFailed, 1, 0); out[kMessageSize] = 'x'; extra = 1;
        }
        encode_message(m, out);
        run.feed(out, kMessageSize + extra);
        ++run.frame;
    }
}

} // namespace

int reload_receiver_target(const std::uint8_t* data, std::size_t size) {
    if (size == 0) return 0;
    Run run;
    if ((data[0] & 1u) == 0) raw_mode(run, data + 1, size - 1);
    else structured_mode(run, data + 1, size - 1);
    run.receiver.detach(run.sink);
    require(run.receiver.state() == ReloadReceiver::State::Idle);
    return 0;
}

} // namespace nnh::fuzz
