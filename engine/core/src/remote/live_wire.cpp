// Live value messages and records (see live_wire.hpp). Pure: only the standard library and pure live headers.

#include "remote/live_wire.hpp"

#include "live/live_names.hpp"

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

/// Whether a Live op takes a hash: 0 required (false) or ≠ 0 required (true); Unfollow takes either.
bool live_fields_ok(LiveOp op, std::uint8_t arg, std::uint16_t count, std::uint32_t hash) {
    switch (op) {
    case LiveOp::Catalogue:
    case LiveOp::Dump: return arg == 0 && count == 0 && hash == 0;
    case LiveOp::Get:
    case LiveOp::Action: return arg == 0 && count == 0 && hash != 0;
    case LiveOp::Follow: return arg >= 1 && hash != 0;
    case LiveOp::Unfollow: return arg == 0 && count == 0;
    case LiveOp::Reset:
        if (count != 0 || arg > static_cast<std::uint8_t>(ResetScope::All)) return false;
        return arg == static_cast<std::uint8_t>(ResetScope::All) ? hash == 0 : hash != 0;
    }
    return false;
}

bool known_kind(std::uint8_t k) { return k >= 1 && k <= live::kLastKind; }

} // namespace

Message live_set_message(std::uint32_t hash, std::uint32_t bits) {
    Message m;
    m.type = MessageType::LiveSet;
    m.press = hash;
    m.release = bits;
    return m;
}

Message live_message(LiveOp op, std::uint8_t arg, std::uint16_t count, std::uint32_t hash) {
    Message m;
    m.type = MessageType::Live;
    m.screens = static_cast<std::uint8_t>(op);
    m.stick = arg;
    m.id = count;
    m.press = hash;
    return m;
}

Message pause_message(PauseOp op, std::uint16_t steps) {
    Message m;
    m.type = MessageType::Pause;
    m.screens = static_cast<std::uint8_t>(op);
    m.id = steps;
    return m;
}

bool decode_live_set_arguments(const std::uint8_t* a, Message& out) {
    out.press = get32(a);
    out.release = get32(a + 4);
    return out.press != 0;
}

bool decode_live_arguments(const std::uint8_t* a, Message& out) {
    out.screens = a[0];
    out.stick = a[1];
    out.id = get16(a + 2);
    out.press = get32(a + 4);
    if (a[0] < static_cast<std::uint8_t>(LiveOp::Catalogue) || a[0] > static_cast<std::uint8_t>(LiveOp::Action)) {
        return false;
    }
    return live_fields_ok(static_cast<LiveOp>(a[0]), out.stick, out.id, out.press);
}

bool decode_pause_arguments(const std::uint8_t* a, Message& out) {
    out.screens = a[0];
    out.id = get16(a + 2);
    for (int i = 4; i < 8; ++i) {
        if (a[i] != 0) return false;
    }
    if (a[1] != 0 || a[0] < 1 || a[0] > 3) return false;
    const bool step = a[0] == static_cast<std::uint8_t>(PauseOp::Step);
    return step ? out.id >= 1 && out.id <= kMaxStepFrames : out.id == 0;
}

void encode_live_arguments(const Message& m, std::uint8_t* a) {
    a[0] = m.screens;
    put16(a + 2, m.id);
    if (m.type == MessageType::Live) {
        a[1] = m.stick;
        put32(a + 4, m.press);
    }
}

std::size_t encode_live_entry_fixed(const LiveEntryHeader& h, std::uint32_t payload_length, std::uint8_t* out) {
    encode_record_header(RecordType::LiveEntry, payload_length, out);
    std::uint8_t* p = out + kRecordHeaderSize;
    put32(p, h.frame);
    put32(p + 4, h.hash);
    p[8] = static_cast<std::uint8_t>(h.kind);
    p[9] = h.flags;
    p[10] = h.name_length;
    p[11] = h.enum_count;
    put32(p + 12, h.value);
    put32(p + 16, h.def);
    put32(p + 20, h.min);
    put32(p + 24, h.max);
    put32(p + 28, h.step);
    return kRecordHeaderSize + kLiveEntryFixed;
}

std::size_t encode_live_entry(const LiveEntryHeader& header, const char* name, const char* const* enum_names,
                              int enum_count, std::uint8_t* out) {
    LiveEntryHeader h = header;
    int name_length = 0;
    while (name_length < 31 && name[name_length] != '\0') ++name_length;
    h.name_length = static_cast<std::uint8_t>(name_length);
    h.enum_count = static_cast<std::uint8_t>(enum_count < 0 ? 0 : (enum_count > 32 ? 32 : enum_count));
    std::size_t at = kRecordHeaderSize + kLiveEntryFixed;
    for (int i = 0; i < name_length; ++i) out[at++] = static_cast<std::uint8_t>(name[i]);
    for (int e = 0; e < h.enum_count; ++e) {
        int n = 0;
        while (n < kMaxEnumNameBytes && enum_names[e][n] != '\0') ++n;
        out[at++] = static_cast<std::uint8_t>(n);
        for (int i = 0; i < n; ++i) out[at++] = static_cast<std::uint8_t>(enum_names[e][i]);
    }
    encode_live_entry_fixed(h, static_cast<std::uint32_t>(at - kRecordHeaderSize), out);
    return at;
}

bool decode_live_entry(const std::uint8_t* p, std::uint32_t length, LiveEntryView& out) {
    if (length < kLiveEntryMin || length > kLiveEntryMax) return false;
    LiveEntryHeader& h = out.header;
    h.frame = get32(p);
    h.hash = get32(p + 4);
    h.kind = static_cast<live::Kind>(p[8]);
    h.flags = p[9];
    h.name_length = p[10];
    h.enum_count = p[11];
    h.value = get32(p + 12);
    h.def = get32(p + 16);
    h.min = get32(p + 20);
    h.max = get32(p + 24);
    h.step = get32(p + 28);
    if (!known_kind(p[8]) || h.name_length == 0 || h.name_length > 31) return false;
    const bool is_enum = h.kind == live::Kind::Enum;
    if (is_enum ? (h.enum_count == 0 || h.enum_count > 32) : h.enum_count != 0) return false;
    std::uint32_t at = kLiveEntryFixed;
    if (at + h.name_length > length) return false;
    out.name = reinterpret_cast<const char*>(p + at);
    if (live::check_name(out.name, h.name_length) != live_name::Error::None) return false;
    at += h.name_length;
    out.enum_names = p + at;
    for (int e = 0; e < h.enum_count; ++e) {
        if (at >= length) return false;
        const std::uint8_t n = p[at];
        if (n == 0 || n > kMaxEnumNameBytes || at + 1 + n > length) return false;
        at += 1u + n;
    }
    return at == length;
}

int live_entry_enum_name(const LiveEntryView& v, int index, char out[kMaxEnumNameBytes + 1]) {
    out[0] = '\0';
    if (index < 0 || index >= v.header.enum_count) return 0;
    const std::uint8_t* p = v.enum_names;
    for (int e = 0; e < index; ++e) p += 1 + p[0];
    const int n = p[0];
    for (int i = 0; i < n; ++i) out[i] = static_cast<char>(p[1 + i]);
    out[n] = '\0';
    return n;
}

std::size_t encode_live_value(const LiveValueRecord& r, std::uint8_t* out) {
    const std::uint32_t length = r.length > 32 ? 32 : r.length;
    encode_record_header(RecordType::LiveValue, kLiveValueFixed + length, out);
    std::uint8_t* p = out + kRecordHeaderSize;
    put32(p, r.frame);
    put32(p + 4, r.hash);
    p[8] = static_cast<std::uint8_t>(r.kind);
    p[9] = r.flags;
    p[10] = static_cast<std::uint8_t>(length);
    p[11] = 0;
    for (std::uint32_t i = 0; i < length; ++i) p[12 + i] = r.value[i];
    return kRecordHeaderSize + kLiveValueFixed + length;
}

bool decode_live_value(const std::uint8_t* p, std::uint32_t length, LiveValueRecord& out) {
    if (length < kLiveValueFixed || length > kLiveValueMax) return false;
    out.frame = get32(p);
    out.hash = get32(p + 4);
    out.kind = static_cast<live::Kind>(p[8]);
    out.flags = p[9];
    out.length = p[10];
    if (!known_kind(p[8]) || p[11] != 0 || kLiveValueFixed + out.length != length) return false;
    const int expected = live::value_bytes(out.kind);
    if (out.kind == live::Kind::Text ? out.length > live::kMaxTextBytes : out.length != expected) return false;
    for (std::uint32_t i = 0; i < out.length; ++i) out.value[i] = p[12 + i];
    return true;
}

std::size_t encode_live_refused(std::uint16_t seq, live::Refusal reason, std::uint32_t hash, std::uint8_t* out) {
    encode_record_header(RecordType::LiveRefused, kLiveRefusedPayload, out);
    std::uint8_t* p = out + kRecordHeaderSize;
    put16(p, seq);
    p[2] = static_cast<std::uint8_t>(reason);
    p[3] = 0;
    put32(p + 4, hash);
    return kRecordHeaderSize + kLiveRefusedPayload;
}

bool decode_live_refused(const std::uint8_t* p, std::uint32_t length, LiveRefusedRecord& out) {
    if (length != kLiveRefusedPayload) return false;
    out.seq = get16(p);
    out.reason = static_cast<live::Refusal>(p[2]);
    out.hash = get32(p + 4);
    return p[2] >= 1 && p[2] <= static_cast<std::uint8_t>(live::Refusal::NoLiveValues) && p[3] == 0;
}

std::size_t encode_live_state(std::uint32_t frame, bool paused, std::uint8_t flags, std::uint16_t steps,
                              std::uint8_t* out) {
    encode_record_header(RecordType::LiveState, kLiveStatePayload, out);
    std::uint8_t* p = out + kRecordHeaderSize;
    put32(p, frame);
    p[4] = paused ? 1 : 0;
    p[5] = flags;
    put16(p + 6, steps);
    return kRecordHeaderSize + kLiveStatePayload;
}

bool decode_live_state(const std::uint8_t* p, std::uint32_t length, LiveStateRecord& out) {
    if (length != kLiveStatePayload || p[4] > 1) return false;
    out.frame = get32(p);
    out.paused = p[4] == 1;
    out.flags = p[5];
    out.steps = get16(p + 6);
    return true;
}

std::size_t encode_live_removed(std::uint32_t frame, std::uint32_t hash, std::uint8_t* out) {
    encode_record_header(RecordType::LiveRemoved, kLiveRemovedPayload, out);
    put32(out + kRecordHeaderSize, frame);
    put32(out + kRecordHeaderSize + 4, hash);
    return kRecordHeaderSize + kLiveRemovedPayload;
}

bool decode_live_removed(const std::uint8_t* p, std::uint32_t length, std::uint32_t& frame, std::uint32_t& hash) {
    if (length != kLiveRemovedPayload) return false;
    frame = get32(p);
    hash = get32(p + 4);
    return true;
}

bool live_record_length_ok(RecordType type, std::uint32_t length) {
    switch (type) {
    case RecordType::LiveEntry: return length >= kLiveEntryMin && length <= kLiveEntryMax;
    case RecordType::LiveRemoved: return length == kLiveRemovedPayload;
    case RecordType::LiveValue: return length >= kLiveValueFixed && length <= kLiveValueMax;
    case RecordType::LiveRefused: return length == kLiveRefusedPayload;
    case RecordType::LiveState: return length == kLiveStatePayload;
    default: return false;
    }
}

} // namespace nnh::detail::remote
