// Records from the game's stream (see game_records.hpp). Layouts: plan 013 "Protocol", core's encoders.

#include "remote/game_records.hpp"

namespace nnh::tool::remote {
namespace {

std::uint16_t get16(const std::uint8_t* p) { return static_cast<std::uint16_t>(p[0] | (p[1] << 8)); }

std::uint32_t get32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

void decode_hello(const std::uint8_t* p, Record& r) {
    r.version = get16(p);
    r.flags = get16(p + 2);
    r.frame = get32(p + 4);
    r.capabilities = get32(p + 8);
    const char* flavor = reinterpret_cast<const char*>(p + 12);
    std::size_t n = 0;
    while (n < 12 && flavor[n] != '\0') ++n;
    r.flavor.assign(flavor, n);
}

void decode_screen(const std::uint8_t* p, std::uint32_t length, Record& r) {
    r.screen.id = get16(p);
    r.screen.screen = p[2];
    r.screen.layout = p[3];
    r.screen.frame = get32(p + 4);
    r.screen.width = get16(p + 8);
    r.screen.height = get16(p + 10);
    r.frame = r.screen.frame;
    r.pixels.assign(p + wire::kScreenFixed, p + length);
}

} // namespace

Record decode_record(const wire::RecordHeader& header, const std::uint8_t* p) {
    Record r;
    r.type = header.type;
    switch (header.type) {
    case wire::RecordType::Hello: decode_hello(p, r); break;
    case wire::RecordType::Tick: r.frame = get32(p); break;
    case wire::RecordType::Late:
        r.seq = get16(p);
        r.wanted = get32(p + 2);
        r.applied = get32(p + 6);
        break;
    case wire::RecordType::Rejected:
        r.seq = get16(p);
        r.reason = static_cast<wire::Reject>(p[2]);
        break;
    case wire::RecordType::Dropped: r.seq = get16(p); break;
    case wire::RecordType::Screen: decode_screen(p, header.length, r); break;
    case wire::RecordType::ScreenFailed:
        r.screen.id = get16(p);
        r.screen.screen = p[2];
        r.frame = r.screen.frame = get32(p + 3);
        break;
    case wire::RecordType::LiveEntry:
    case wire::RecordType::LiveRemoved:
    case wire::RecordType::LiveValue:
    case wire::RecordType::LiveRefused:
    case wire::RecordType::LiveState:
        r.payload.assign(p, p + header.length);
        if (header.type != wire::RecordType::LiveRefused) r.frame = get32(p);
        break;
    case wire::RecordType::ReloadStaged:  // spec 031: the payload is decoded by reload_wire.hpp's decoders
    case wire::RecordType::ReloadApplied:
    case wire::RecordType::ReloadRefused:
        r.payload.assign(p, p + header.length);
        r.frame = get32(p + (header.type == wire::RecordType::ReloadRefused ? 8 : 4));
        break;
    }
    return r;
}

} // namespace nnh::tool::remote
