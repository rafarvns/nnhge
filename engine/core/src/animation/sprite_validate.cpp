// Bounded validation of a `.nnhsprite` file (see sprite_validate.hpp).

#include "animation/sprite_validate.hpp"

#include <cstring>

#include "animation/sprite_format.hpp"
#include "animation/sprite_layout.hpp"
#include "animation/sprite_sequence.hpp"
#include "core/crc32.hpp"
#include "nnh/detail/name_hash.hpp"
#include "text/utf8.hpp"

namespace nnh::detail {

namespace {

using namespace spritefmt;

constexpr std::size_t kCrcStart = 16;

SpriteCheck damaged(const char* why) {
    return {SpriteProblem::Damaged, why};
}

/// Copies record `i` of section `s` (the file may be unaligned).
struct Reader {
    const std::uint8_t* data;
    Header h;
    template <typename T>
    T at(int s, int i) const {
        T out;
        const std::uint32_t base = s == kFrames ? sizeof(Header) : h.offset[s - 1];
        std::memcpy(&out, data + base + static_cast<std::size_t>(i) * sizeof(T), sizeof(T));
        return out;
    }
};

SpriteCheck check_header(const std::uint8_t* bytes, std::size_t size, Header& h) {
    if (bytes == nullptr || size < sizeof(Header)) return damaged("too short to be a sprite library");
    std::memcpy(&h, bytes, sizeof h);
    if (std::memcmp(h.magic, kMagic, sizeof h.magic) != 0) return damaged("not a sprite library (no NNHA magic)");
    if (h.version > kVersion) return {SpriteProblem::NewerVersion, "made by a newer engine (format version)"};
    if (h.version != kVersion) return damaged("unknown format version");
    if (h.file_size != size) return damaged("file size does not match its header (truncated?)");
    if (sprite_file_crc(bytes, size) != h.crc) return damaged("CRC mismatch: the file is damaged");
    SpriteCapacities caps;
    for (int s = 0; s < kSectionCount; ++s) caps.cap[s] = h.cap[s];
    if (h.cap[kFrames] == 0) return damaged("a library without frame capacity");
    if (check_sprite_capacities(caps) != nullptr) return {SpriteProblem::OverLimit, "a capacity is over its ceiling"};
    const SpriteLayout layout = sprite_layout(caps);
    if (layout.total != size) return damaged("block size does not match the capacities");
    for (int s = 1; s < kSectionCount; ++s) {
        if (layout.offset[s] != h.offset[s - 1]) return damaged("section offsets do not match the capacities");
    }
    for (int s = 0; s < kSectionCount; ++s) {
        if (h.count[s] > h.cap[s]) return damaged("a count is over its capacity");
    }
    if (h.pivot > kBottomCenter) return damaged("unknown default pivot");
    if (h.reserved[0] != 0 || h.reserved[1] != 0 || h.reserved[2] != 0) return damaged("reserved header bytes set");
    return {};
}

SpriteCheck check_frames(const Reader& r) {
    for (int i = 0; i < r.h.count[kFrames]; ++i) {
        const FrameEntry f = r.at<FrameEntry>(kFrames, i);
        if (f.w < 1 || f.h < 1 || f.x < 0 || f.y < 0) return damaged("frame region empty or negative");
        if (f.x > 32767 - f.w || f.y > 32767 - f.h) return damaged("frame region beyond 32767 pixels");
        if (f.offset_x < 0 || f.offset_y < 0) return damaged("negative trim offset");
    }
    return {};
}

/// A NUL-terminated, zero-padded name of 1..31 bytes of valid UTF-8 whose hash is `hash`.
bool name_ok(const char (&name)[32], std::uint32_t hash) {
    std::size_t length = 0;
    while (length < 32 && name[length] != '\0') ++length;
    if (length == 0 || length > static_cast<std::size_t>(kMaxNameLength)) return false;
    for (std::size_t i = length; i < 32; ++i) {
        if (name[i] != '\0') return false;
    }
    return utf8_valid(name, length) && fnv1a32(name, length) == hash;
}

int stored_count_ok(const ClipEntry& c) {
    switch (c.mode) {
    case kLoop: return c.count == 0;
    case kOnce: return c.count == 1;
    case kRepeat: return c.count >= 1;
    case kPingPong: return 1;
    default: return 0;
    }
}

SpriteCheck check_clip(const Reader& r, const ClipEntry& c, int next_first) {
    if (!name_ok(c.name, c.hash)) return damaged("clip name or hash damaged");
    if (c.first != next_first) return damaged("clip frames do not follow each other");
    if (c.length < 1 || c.length > kMaxClipLength) return damaged("clip length outside 1..256");
    if (c.first + c.length > r.h.count[kClipFrames]) return damaged("clip frames outside the clip frame table");
    if (c.direction > kReverse || !stored_count_ok(c)) return damaged("clip mode, direction or count damaged");
    const SpriteSequenceEnd end = sprite_sequence_end(c.length, c.mode, c.count);
    if (c.end_step != end.end_step || c.end_cycle != end.end_cycle) return damaged("clip end does not match its mode");
    std::int64_t cycle = 0;
    const int length = sprite_cycle_length(c.length, c.mode);
    for (int p = 0; p < length; ++p) {
        const int index = sprite_clip_index(c.length, c.mode, c.direction, p);
        cycle += r.at<ClipFrameEntry>(kClipFrames, c.first + index).duration;
    }
    if (cycle != c.cycle_ticks || cycle > kMaxTicks) return damaged("clip duration damaged or over 1 hour");
    return {};
}

SpriteCheck check_clip_frames(const Reader& r) {
    int next_marker = 0;
    for (int i = 0; i < r.h.count[kClipFrames]; ++i) {
        const ClipFrameEntry f = r.at<ClipFrameEntry>(kClipFrames, i);
        if (f.duration < 1 || f.duration > kMaxTicks) return damaged("clip frame duration outside 1 tick..1 hour");
        if (f.frame >= r.h.count[kFrames]) return damaged("clip frame shows an unknown sheet frame");
        if (f.first_marker != next_marker || f.markers > kMaxMarkersPerFrame) return damaged("marker runs damaged");
        if (f.pad[0] != 0 || f.pad[1] != 0 || f.pad[2] != 0) return damaged("clip frame padding set");
        next_marker += f.markers;
    }
    if (next_marker != r.h.count[kMarkers]) return damaged("marker runs do not add up to the markers");
    return {};
}

SpriteCheck check_clips(const Reader& r) {
    int next_first = 0;
    for (int i = 0; i < r.h.count[kClips]; ++i) {
        const ClipEntry c = r.at<ClipEntry>(kClips, i);
        const SpriteCheck one = check_clip(r, c, next_first);
        if (!one.ok()) return one;
        for (int j = 0; j < i; ++j) {
            if (r.at<ClipEntry>(kClips, j).hash == c.hash) return damaged("two clips with the same name hash");
        }
        next_first += c.length;
    }
    if (next_first != r.h.count[kClipFrames]) return damaged("clips do not add up to the clip frames");
    return {};
}

SpriteCheck check_states(const Reader& r) {
    const int states = r.h.count[kStates];
    for (int i = 0; i < states; ++i) {
        const StateEntry s = r.at<StateEntry>(kStates, i);
        if (!name_ok(s.name, s.hash)) return damaged("state name or hash damaged");
        if (s.clip >= r.h.count[kClips]) return damaged("state plays an unknown clip");
        if (s.next != kNoState && s.next >= states) return damaged("next state on finish unknown");
        if (s.pad != 0) return damaged("state padding set");
        for (int j = 0; j < i; ++j) {
            if (r.at<StateEntry>(kStates, j).hash == s.hash) return damaged("two states with the same name hash");
        }
    }
    for (int i = 0; i < r.h.count[kTransitions]; ++i) {
        const TransitionEntry t = r.at<TransitionEntry>(kTransitions, i);
        const bool from_ok = t.from == kAnyState || t.from < states;
        const bool to_ok = t.to == kAnyState || t.to < states;
        if (!from_ok || !to_ok || t.when > kNever || t.keep_time > 1) return damaged("transition rule damaged");
        for (int j = 0; j < i; ++j) {
            const TransitionEntry o = r.at<TransitionEntry>(kTransitions, j);
            if (o.from == t.from && o.to == t.to) return damaged("two rules for the same states");
        }
    }
    return {};
}

} // namespace

std::uint32_t sprite_file_crc(const void* data, std::size_t size) {
    if (size <= kCrcStart) return 0;
    return crc32(static_cast<const std::uint8_t*>(data) + kCrcStart, size - kCrcStart);
}

SpriteCheck validate_sprite_library(const void* data, std::size_t size) {
    Reader r{static_cast<const std::uint8_t*>(data), {}};
    SpriteCheck c = check_header(r.data, size, r.h);
    if (c.ok()) c = check_frames(r);
    if (c.ok()) c = check_clip_frames(r);
    if (c.ok()) c = check_clips(r);
    if (c.ok()) c = check_states(r);
    return c;
}

} // namespace nnh::detail
