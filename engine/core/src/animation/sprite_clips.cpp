// Clips (spec 016 FR-3, FR-5; T2.2): names, frame lists, durations on the frame clock, modes and their ends, and the
// clip queries.

#include "animation/sprite_library_internal.hpp"
#include "animation/sprite_sequence.hpp"

namespace nnh {

namespace {

using namespace detail::spritefmt;

/// The stored count of a clip (Once 1, Repeat plays, PingPong passes, Loop 0), or -1 with `why`.
int clip_count_of(const SpriteClipDesc& c, const char*& why) {
    switch (c.mode) {
    case SpriteMode::Loop:
        if (c.repeat != 0 || c.pingpong_passes != 0) why = "repeat and pingpong_passes are for Repeat and PingPong";
        return 0;
    case SpriteMode::Once:
        if (c.repeat > 1 || c.repeat < 0 || c.pingpong_passes != 0) why = "a Once clip plays once (use Repeat)";
        return 1;
    case SpriteMode::Repeat:
        if (c.repeat < 1 || c.repeat > kMaxRepeat) why = "repeat must be 1..65535 for a Repeat clip";
        return c.repeat;
    case SpriteMode::PingPong:
        if (c.pingpong_passes != 0) {
            if (c.pingpong_passes < 0 || c.pingpong_passes > kMaxRepeat) why = "pingpong_passes must be 0..65535";
            return c.pingpong_passes;
        }
        if (c.repeat < 0 || c.repeat > kMaxRepeat / 2) why = "repeat (round trips) must be 0..32767";
        return 2 * c.repeat;
    }
    why = "unknown mode";
    return -1;
}

std::int32_t duration_of(const SpriteClipDesc& c, int i) {
    return c.durations != nullptr ? c.durations[i].ticks : c.frame_time.ticks;
}

/// Checks everything but capacity; nullptr when the clip is fine.
const char* clip_problem(const detail::SpriteView& v, const SpriteClipDesc& c) {
    if (const char* why = detail::sprite_name_problem(c.name)) return why;
    const std::uint32_t hash = detail::fnv1a32(c.name);
    for (int i = 0; i < v.count(kClips); ++i) {
        if (v.clips()[i].hash == hash) return "a clip has this name already (or the same name hash)";
    }
    if (c.count < 1 || c.count > kMaxClipLength) return "count must be 1..256";
    if (static_cast<int>(c.direction) > kReverse) return "unknown direction";
    for (int i = 0; i < c.count; ++i) {
        const int frame = c.frames != nullptr ? c.frames[i] : c.first + i;
        if (frame < 0 || frame >= v.count(kFrames)) return "a frame index is not a frame of the library";
        const std::int32_t d = duration_of(c, i);
        if (d < 1 || d > kMaxTicks) return "a frame duration is zero, negative or over 1 hour";
    }
    return nullptr;
}

} // namespace

Result<SpriteClipId> SpriteLibrary::add_clip(const SpriteClipDesc& c) {
    if (block_ == nullptr) return Error::make(ErrorCode::InvalidArgument, "add_clip: empty library");
    const detail::SpriteView v = detail::sprite_view(block_);
    const char* name = c.name != nullptr ? c.name : "";
    if (const char* why = clip_problem(v, c)) return Error::make(ErrorCode::InvalidArgument, "clip \"%s\": %s", name, why);
    const char* why = nullptr;
    const int count = clip_count_of(c, why);
    if (why != nullptr) return Error::make(ErrorCode::InvalidArgument, "clip \"%s\": %s", name, why);
    if (v.count(kClips) >= v.cap(kClips)) {
        return Error::make(ErrorCode::LimitReached, "clip \"%s\": the library holds %d clips", name, v.cap(kClips));
    }
    const int first = v.count(kClipFrames);
    if (c.count > v.cap(kClipFrames) - first) {
        return Error::make(ErrorCode::LimitReached, "clip \"%s\": the library holds %d clip frames", name,
                           v.cap(kClipFrames));
    }
    const auto mode = static_cast<std::uint8_t>(c.mode);
    const auto direction = static_cast<std::uint8_t>(c.direction);
    const int length = detail::sprite_cycle_length(c.count, mode);
    std::int64_t cycle = 0;
    for (int p = 0; p < length; ++p) cycle += duration_of(c, detail::sprite_clip_index(c.count, mode, direction, p));
    if (cycle > kMaxTicks) return Error::make(ErrorCode::InvalidArgument, "clip \"%s\": one play is over 1 hour", name);

    for (int i = 0; i < c.count; ++i) {
        ClipFrameEntry& f = v.clip_frames()[first + i];
        f = {};
        f.duration = duration_of(c, i);
        f.frame = static_cast<std::uint16_t>(c.frames != nullptr ? c.frames[i] : c.first + i);
        f.first_marker = static_cast<std::uint16_t>(v.count(kMarkers));
    }
    const int index = v.count(kClips);
    ClipEntry& e = v.clips()[index];
    e = {};
    detail::sprite_store_name(e.name, c.name);
    e.hash = detail::fnv1a32(c.name);
    e.first = static_cast<std::uint16_t>(first);
    e.length = static_cast<std::uint16_t>(c.count);
    e.mode = mode;
    e.direction = direction;
    e.count = static_cast<std::uint16_t>(count);
    const detail::SpriteSequenceEnd end = detail::sprite_sequence_end(c.count, mode, count);
    e.end_step = end.end_step;
    e.end_cycle = end.end_cycle;
    e.cycle_ticks = static_cast<std::int32_t>(cycle);
    v.header().count[kClipFrames] = static_cast<std::uint16_t>(first + c.count);
    v.header().count[kClips] = static_cast<std::uint16_t>(index + 1);
    return SpriteClipId{static_cast<std::uint16_t>(index)};
}

SpriteClipId detail::sprite_find_clip(const SpriteView& v, const char* name) {
    if (v.block == nullptr || name == nullptr) return {};
    const std::uint32_t hash = fnv1a32(name);
    for (int i = 0; i < v.count(kClips); ++i) {
        if (v.clips()[i].hash == hash) return SpriteClipId{static_cast<std::uint16_t>(i)};
    }
    return {};
}

SpriteClipId SpriteLibrary::find_clip(const char* name) const {
    return detail::sprite_find_clip(detail::sprite_view(block_), name);
}

const char* SpriteLibrary::clip_name(SpriteClipId clip) const {
    if (block_ == nullptr || clip.value >= clip_count()) return "";
    return detail::sprite_view(block_).clips()[clip.value].name;
}

int SpriteLibrary::clip_length(SpriteClipId clip) const {
    if (block_ == nullptr || clip.value >= clip_count()) return 0;
    return detail::sprite_view(block_).clips()[clip.value].length;
}

FrameTime SpriteLibrary::clip_duration(SpriteClipId clip) const {
    if (block_ == nullptr || clip.value >= clip_count()) return {};
    return FrameTime::from_ticks(detail::sprite_view(block_).clips()[clip.value].cycle_ticks);
}

} // namespace nnh
