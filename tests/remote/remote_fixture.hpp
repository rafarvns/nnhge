#pragma once
// Helpers for the console remote session tests: an in-memory PC (the fake platform) that sends messages and
// reads back what the console sent, split into records. Decoding is done here by hand, not with the code
// under test.

#include <cstdint>
#include <cstring>

#include "nnh/log.hpp"
#include "nnh/platform_fake.hpp"
#include "remote/remote_wire.hpp"
#include "test.hpp"

namespace remote_test {

namespace wire = nnh::detail::remote;
namespace fake = nnh::platform_fake;

inline std::uint16_t le16(const std::uint8_t* p) { return static_cast<std::uint16_t>(p[0] | (p[1] << 8)); }
inline std::uint32_t le32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

/// Sends one message from the PC.
inline void send(const wire::Message& m) {
    std::uint8_t bytes[wire::kMessageSize];
    wire::encode_message(m, bytes);
    fake::remote_push(bytes, sizeof bytes);
}

inline wire::Message hello(std::uint16_t seq = 1) {
    wire::Message m;
    m.type = wire::MessageType::Hello;
    m.seq = seq;
    m.version = wire::kVersion;
    return m;
}

inline wire::Message buttons(std::uint16_t seq, std::uint32_t frame, std::uint32_t press, std::uint32_t release = 0) {
    wire::Message m;
    m.type = wire::MessageType::Buttons;
    m.seq = seq;
    m.frame = frame;
    m.press = press;
    m.release = release;
    return m;
}

inline wire::Message stick(std::uint16_t seq, std::uint32_t frame, std::uint8_t which, std::int16_t x, std::int16_t y) {
    wire::Message m;
    m.type = wire::MessageType::Stick;
    m.seq = seq;
    m.frame = frame;
    m.stick = which;
    m.x = x;
    m.y = y;
    return m;
}

inline wire::Message touch(std::uint16_t seq, std::uint32_t frame, bool down, std::int16_t x, std::int16_t y) {
    wire::Message m;
    m.type = wire::MessageType::Touch;
    m.seq = seq;
    m.frame = frame;
    m.down = down;
    m.x = x;
    m.y = y;
    return m;
}

inline wire::Message screenshot(std::uint16_t seq, std::uint32_t frame, std::uint16_t id, std::uint8_t screens) {
    wire::Message m;
    m.type = wire::MessageType::Screenshot;
    m.seq = seq;
    m.frame = frame;
    m.id = id;
    m.screens = screens;
    return m;
}

inline wire::Message bye(std::uint16_t seq, std::uint32_t frame, bool quit = false) {
    wire::Message m;
    m.type = wire::MessageType::Bye;
    m.seq = seq;
    m.frame = frame;
    m.quit = quit;
    return m;
}

/// The 3D slider (spec 026 FR-16): `thousandths` 0..1000, or released.
inline wire::Message slider(std::uint16_t seq, std::uint32_t frame, std::uint16_t thousandths, bool release = false) {
    wire::Message m;
    m.type = wire::MessageType::Slider;
    m.seq = seq;
    m.frame = frame;
    m.slider = release ? wire::kSliderRelease : thousandths;
    return m;
}

/// The system language (spec 029 FR-27): a code (at most 8 bytes), or nullptr for `language off`.
inline wire::Message language(std::uint16_t seq, std::uint32_t frame, const char* code) {
    wire::Message m;
    m.type = wire::MessageType::Language;
    m.seq = seq;
    m.frame = frame;
    CHECK(wire::pack_language(code, m));
    return m;
}

/// 16 bytes with a bad magic and the given seq (Rejected{seq, Magic}).
inline void send_garbage(std::uint16_t seq) {
    std::uint8_t bytes[wire::kMessageSize] = {0x00, 2, static_cast<std::uint8_t>(seq),
                                              static_cast<std::uint8_t>(seq >> 8)};
    fake::remote_push(bytes, sizeof bytes);
}

struct Record {
    wire::RecordType type = wire::RecordType::Tick;
    std::uint32_t length = 0;
    const std::uint8_t* payload = nullptr;
};

/// Everything the console sent, split into records. `whole` is false if a byte was not part of a record.
struct Sent {
    static constexpr int kMax = 512;
    int count = 0;
    Record records[kMax];
    bool whole = true;

    int count_of(wire::RecordType type) const {
        int n = 0;
        for (int i = 0; i < count; ++i) n += records[i].type == type ? 1 : 0;
        return n;
    }
    /// The `nth` (0-based) record of `type`, or nullptr.
    const Record* find(wire::RecordType type, int nth = 0) const {
        for (int i = 0; i < count; ++i) {
            if (records[i].type == type && nth-- == 0) return &records[i];
        }
        return nullptr;
    }
};

inline Sent sent() {
    Sent s;
    std::size_t size = 0;
    const std::uint8_t* data = fake::remote_sent(size);
    std::size_t at = 0;
    while (at < size) {
        wire::RecordHeader h;
        if (size - at < wire::kRecordHeaderSize || !wire::decode_record_header(data + at, h) ||
            size - at - wire::kRecordHeaderSize < h.length || s.count == Sent::kMax) {
            s.whole = false;
            break;
        }
        s.records[s.count++] = {h.type, h.length, data + at + wire::kRecordHeaderSize};
        at += wire::kRecordHeaderSize + h.length;
    }
    return s;
}

/// Counts the log lines of channel `remote` at a level while alive; keeps the last one.
struct RemoteLines {
    nnh::LogLevel level;
    int count = 0;
    char last[nnh::log::kMaxLineLength] = {};

    explicit RemoteLines(nnh::LogLevel l) : level(l) { nnh::log::add_sink(&RemoteLines::sink, this); }
    ~RemoteLines() { nnh::log::remove_sink(&RemoteLines::sink, this); }
    RemoteLines(const RemoteLines&) = delete;
    RemoteLines& operator=(const RemoteLines&) = delete;

    static void sink(nnh::LogLevel l, const char* line, void* user) {
        auto* self = static_cast<RemoteLines*>(user);
        if (l != self->level || std::strstr(line, " remote ") == nullptr) return;
        ++self->count;
        std::strncpy(self->last, line, sizeof self->last - 1);
    }
};

} // namespace remote_test
