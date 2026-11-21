// Console remote wire format (see remote_wire.hpp). Pure: only the standard library, shared with tools/nnh.

#include "remote/remote_wire.hpp"

namespace nnh::detail::remote {
namespace {

std::uint16_t get16(const std::uint8_t* p) { return static_cast<std::uint16_t>(p[0] | (p[1] << 8)); }

std::uint32_t get32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

void put16(std::uint8_t* p, std::uint16_t v) {
    p[0] = static_cast<std::uint8_t>(v);
    p[1] = static_cast<std::uint8_t>(v >> 8);
}

void put32(std::uint8_t* p, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) p[i] = static_cast<std::uint8_t>(v >> (8 * i));
}

/// Bytes [from, kMessageSize) are all zero.
bool zero_from(const std::uint8_t* in, std::size_t from) {
    for (std::size_t i = from; i < kMessageSize; ++i) {
        if (in[i] != 0) return false;
    }
    return true;
}

bool stick_axis_ok(std::int16_t v) { return v >= -kStickMax && v <= kStickMax; }

/// Validates the argument bytes (8–15) of a message whose type is known, filling `out`.
bool decode_arguments(const std::uint8_t* in, Message& out) {
    const std::uint8_t* a = in + 8;
    switch (out.type) {
    case MessageType::Hello:
        out.version = get16(a);
        return out.version == kVersion && get16(a + 2) == 0 && zero_from(in, 12);
    case MessageType::Buttons:
        out.press = get32(a);
        out.release = get32(a + 4);
        return ((out.press | out.release) & ~kButtonMask) == 0 && (out.press & out.release) == 0;
    case MessageType::Stick:
        out.stick = a[0];
        out.x = static_cast<std::int16_t>(get16(a + 2));
        out.y = static_cast<std::int16_t>(get16(a + 4));
        return out.stick <= 1 && a[1] == 0 && stick_axis_ok(out.x) && stick_axis_ok(out.y) && zero_from(in, 14);
    case MessageType::Touch: {
        const std::uint16_t x = get16(a + 2), y = get16(a + 4);
        out.down = a[0] == 1;
        out.x = static_cast<std::int16_t>(x);
        out.y = static_cast<std::int16_t>(y);
        return a[0] <= 1 && a[1] == 0 && x <= kTouchMaxX && y <= kTouchMaxY && zero_from(in, 14);
    }
    case MessageType::Screenshot:
        out.id = get16(a);
        out.screens = a[2];
        return out.screens != 0 && (out.screens & ~(kScreenTop | kScreenBottom)) == 0 && zero_from(in, 11);
    case MessageType::Bye:
        out.quit = a[0] == 1;
        return a[0] <= 1 && zero_from(in, 9);
    }
    return false;
}

/// Payload length allowed for a record type: exact, or a range for Screen. False for unknown types.
bool record_length_ok(std::uint8_t type, std::uint32_t length) {
    switch (static_cast<RecordType>(type)) {
    case RecordType::Hello: return length == kHelloPayload;
    case RecordType::Tick: return length == kTickPayload;
    case RecordType::Late: return length == kLatePayload;
    case RecordType::Rejected: return length == kRejectedPayload;
    case RecordType::Dropped: return length == kDroppedPayload;
    case RecordType::Screen: return length >= kScreenFixed && length <= kMaxRecordPayload;
    case RecordType::ScreenFailed: return length == kScreenFailedPayload;
    }
    return false;
}

} // namespace

void encode_message(const Message& m, std::uint8_t out[kMessageSize]) {
    for (std::size_t i = 0; i < kMessageSize; ++i) out[i] = 0;
    out[0] = kMessageMagic;
    out[1] = static_cast<std::uint8_t>(m.type);
    put16(out + 2, m.seq);
    put32(out + 4, m.frame);
    std::uint8_t* a = out + 8;
    switch (m.type) {
    case MessageType::Hello: put16(a, m.version); break;
    case MessageType::Buttons:
        put32(a, m.press);
        put32(a + 4, m.release);
        break;
    case MessageType::Stick:
    case MessageType::Touch:
        a[0] = m.type == MessageType::Stick ? m.stick : static_cast<std::uint8_t>(m.down ? 1 : 0);
        put16(a + 2, static_cast<std::uint16_t>(m.x));
        put16(a + 4, static_cast<std::uint16_t>(m.y));
        break;
    case MessageType::Screenshot:
        put16(a, m.id);
        a[2] = m.screens;
        break;
    case MessageType::Bye: a[0] = m.quit ? 1 : 0; break;
    }
}

Reject decode_message(const std::uint8_t in[kMessageSize], Message& out) {
    if (in[0] != kMessageMagic) return Reject::Magic;
    if (in[1] < static_cast<std::uint8_t>(MessageType::Hello) || in[1] > static_cast<std::uint8_t>(MessageType::Bye)) {
        return Reject::Type;
    }
    out = Message{};
    out.type = static_cast<MessageType>(in[1]);
    out.seq = get16(in + 2);
    out.frame = get32(in + 4);
    return decode_arguments(in, out) ? Reject::None : Reject::Range;
}

std::uint16_t message_seq(const std::uint8_t in[kMessageSize]) { return get16(in + 2); }

void encode_record_header(RecordType type, std::uint32_t length, std::uint8_t out[kRecordHeaderSize]) {
    out[0] = kRecordMark[0];
    out[1] = kRecordMark[1];
    out[2] = kRecordMark[2];
    out[3] = static_cast<std::uint8_t>(type);
    put32(out + 4, length);
}

bool decode_record_header(const std::uint8_t in[kRecordHeaderSize], RecordHeader& out) {
    if (in[0] != kRecordMark[0] || in[1] != kRecordMark[1] || in[2] != kRecordMark[2]) return false;
    const std::uint32_t length = get32(in + 4);
    if (!record_length_ok(in[3], length)) return false;
    out.type = static_cast<RecordType>(in[3]);
    out.length = length;
    return true;
}

std::size_t encode_hello_record(std::uint32_t frame, std::uint32_t capabilities, const char* flavor,
                                std::uint8_t* out) {
    encode_record_header(RecordType::Hello, kHelloPayload, out);
    std::uint8_t* p = out + kRecordHeaderSize;
    put16(p, kVersion);
    put16(p + 2, 0);
    put32(p + 4, frame);
    put32(p + 8, capabilities);
    bool ended = false;
    for (int i = 0; i < 12; ++i) {
        ended = ended || flavor[i] == '\0';
        p[12 + i] = ended ? 0 : static_cast<std::uint8_t>(flavor[i]);
    }
    return kRecordHeaderSize + kHelloPayload;
}

std::size_t encode_tick_record(std::uint32_t frame, std::uint8_t* out) {
    encode_record_header(RecordType::Tick, kTickPayload, out);
    put32(out + kRecordHeaderSize, frame);
    return kRecordHeaderSize + kTickPayload;
}

std::size_t encode_late_record(std::uint16_t seq, std::uint32_t wanted, std::uint32_t applied, std::uint8_t* out) {
    encode_record_header(RecordType::Late, kLatePayload, out);
    put16(out + kRecordHeaderSize, seq);
    put32(out + kRecordHeaderSize + 2, wanted);
    put32(out + kRecordHeaderSize + 6, applied);
    return kRecordHeaderSize + kLatePayload;
}

std::size_t encode_rejected_record(std::uint16_t seq, Reject reason, std::uint8_t* out) {
    encode_record_header(RecordType::Rejected, kRejectedPayload, out);
    put16(out + kRecordHeaderSize, seq);
    out[kRecordHeaderSize + 2] = static_cast<std::uint8_t>(reason);
    return kRecordHeaderSize + kRejectedPayload;
}

std::size_t encode_dropped_record(std::uint16_t seq, std::uint8_t* out) {
    encode_record_header(RecordType::Dropped, kDroppedPayload, out);
    put16(out + kRecordHeaderSize, seq);
    return kRecordHeaderSize + kDroppedPayload;
}

std::size_t encode_screen_record(const ScreenHeader& h, std::uint32_t pixel_bytes, std::uint8_t* out) {
    encode_record_header(RecordType::Screen, kScreenFixed + pixel_bytes, out);
    std::uint8_t* p = out + kRecordHeaderSize;
    put16(p, h.id);
    p[2] = h.screen;
    p[3] = h.layout;
    put32(p + 4, h.frame);
    put16(p + 8, h.width);
    put16(p + 10, h.height);
    return kRecordHeaderSize + kScreenFixed;
}

std::size_t encode_screen_failed_record(std::uint16_t id, std::uint8_t screen, std::uint32_t frame,
                                        std::uint8_t* out) {
    encode_record_header(RecordType::ScreenFailed, kScreenFailedPayload, out);
    std::uint8_t* p = out + kRecordHeaderSize;
    put16(p, id);
    p[2] = screen;
    put32(p + 3, frame);
    return kRecordHeaderSize + kScreenFailedPayload;
}

} // namespace nnh::detail::remote
