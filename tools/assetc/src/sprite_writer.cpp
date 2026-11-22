// Writing a sprite model as a `.nnhsprite` block (see sprite_writer.hpp).

#include "sprite_writer.hpp"

#include <cmath>
#include <cstring>

#include "animation/sprite_format.hpp"
#include "animation/sprite_layout.hpp"
#include "animation/sprite_sequence.hpp"
#include "animation/sprite_validate.hpp"
#include "nnh/detail/name_hash.hpp"

namespace nnh::assetc {

namespace {

using namespace nnh::detail::spritefmt;

bool fail(Problem& problem, const std::string& file, int line, const std::string& reason) {
    problem = {file, reason, line};
    return false;
}

struct Tables {
    std::vector<ClipFrameEntry> clip_frames;
    std::vector<MarkerEntry> markers;
    std::vector<ClipEntry> clips;
};

/// Clip frames and their markers, clip by clip.
bool place_clips(const SpriteModel& m, const std::string& file, Tables& t, Problem& problem) {
    for (const SpriteClipModel& c : m.clips) {
        ClipEntry e{};
        std::memcpy(e.name, c.name.data(), c.name.size());
        e.hash = nnh::detail::fnv1a32(c.name.c_str());
        e.first = static_cast<std::uint16_t>(t.clip_frames.size());
        e.length = static_cast<std::uint16_t>(c.to - c.from + 1);
        e.mode = c.mode;
        e.direction = c.direction;
        e.count = static_cast<std::uint16_t>(c.count);
        if (t.clip_frames.size() + e.length > static_cast<std::size_t>(kMaxClipFrames)) {
            return fail(problem, file, c.line, "the clips have more than 8192 frames in all");
        }
        for (int frame = c.from; frame <= c.to; ++frame) {
            ClipFrameEntry f{};
            f.duration = m.frames[static_cast<std::size_t>(frame)].duration;
            f.frame = static_cast<std::uint16_t>(frame);
            f.first_marker = static_cast<std::uint16_t>(t.markers.size());
            for (const SpriteMarkerModel& marker : m.markers) {
                if (frame < marker.from || frame > marker.to) continue;
                if (f.markers == kMaxMarkersPerFrame) {
                    return fail(problem, file, marker.line, "more than 8 markers on frame " + std::to_string(frame));
                }
                if (t.markers.size() == static_cast<std::size_t>(kMaxMarkers)) {
                    return fail(problem, file, marker.line, "more than 1024 markers on the clips' frames");
                }
                t.markers.push_back({nnh::detail::fnv1a32(marker.name.c_str()), marker.value});
                ++f.markers;
            }
            t.clip_frames.push_back(f);
        }
        const nnh::detail::SpriteSequenceEnd end = nnh::detail::sprite_sequence_end(e.length, e.mode, e.count);
        e.end_step = end.end_step;
        e.end_cycle = end.end_cycle;
        std::int64_t cycle = 0;
        const int length = nnh::detail::sprite_cycle_length(e.length, e.mode);
        for (int p = 0; p < length; ++p) {
            cycle += t.clip_frames[e.first + nnh::detail::sprite_clip_index(e.length, e.mode, e.direction, p)].duration;
        }
        if (cycle > kMaxTicks) return fail(problem, file, c.line, "clip \"" + c.name + "\" lasts over 1 hour");
        e.cycle_ticks = static_cast<std::int32_t>(cycle);
        t.clips.push_back(e);
    }
    return true;
}

/// Two different marker names with the same hash can't be told apart on the console.
bool check_marker_names(const SpriteModel& m, const std::string& file, Problem& problem) {
    for (std::size_t i = 0; i < m.markers.size(); ++i) {
        for (std::size_t j = 0; j < i; ++j) {
            if (m.markers[i].name != m.markers[j].name &&
                nnh::detail::fnv1a32(m.markers[i].name.c_str()) == nnh::detail::fnv1a32(m.markers[j].name.c_str())) {
                return fail(problem, file, m.markers[i].line, "markers \"" + m.markers[i].name + "\" and \"" +
                                                                  m.markers[j].name + "\" have the same name hash");
            }
        }
    }
    return true;
}

template <typename T>
void put(std::vector<std::uint8_t>& out, std::uint32_t at, const std::vector<T>& records) {
    if (!records.empty()) std::memcpy(out.data() + at, records.data(), records.size() * sizeof(T));
}

} // namespace

bool write_sprite(const SpriteModel& m, const std::string& file, std::vector<std::uint8_t>& out, Problem& problem) {
    Tables t;
    if (!check_marker_names(m, file, problem) || !place_clips(m, file, t, problem)) return false;
    std::int16_t pivot_x = 0, pivot_y = 0;
    if (m.has_pivot) {
        if (std::fabs(m.pivot_x) > kMaxPivot || std::fabs(m.pivot_y) > kMaxPivot) {
            return fail(problem, file, m.pivot_line, "the pivot slice is more than 2047 pixels from the frame");
        }
        pivot_x = static_cast<std::int16_t>(std::lround(m.pivot_x * 16));
        pivot_y = static_cast<std::int16_t>(std::lround(m.pivot_y * 16));
    }
    std::vector<FrameEntry> frames;
    for (const SpriteFrameModel& f : m.frames) {
        frames.push_back({static_cast<std::int16_t>(f.x), static_cast<std::int16_t>(f.y), static_cast<std::int16_t>(f.w),
                          static_cast<std::int16_t>(f.h), pivot_x, pivot_y, static_cast<std::int16_t>(f.offset_x),
                          static_cast<std::int16_t>(f.offset_y)});
    }
    nnh::detail::SpriteCapacities caps;
    caps.cap[kFrames] = static_cast<int>(frames.size());
    caps.cap[kClips] = static_cast<int>(t.clips.size());
    caps.cap[kClipFrames] = static_cast<int>(t.clip_frames.size());
    caps.cap[kMarkers] = static_cast<int>(t.markers.size());
    const nnh::detail::SpriteLayout layout = nnh::detail::sprite_layout(caps);
    out.assign(layout.total, 0);
    Header h{};
    std::memcpy(h.magic, kMagic, sizeof h.magic);
    h.version = kVersion;
    h.file_size = layout.total;
    for (int s = 0; s < kSectionCount; ++s) {
        h.count[s] = h.cap[s] = static_cast<std::uint16_t>(caps.cap[s]);
        if (s > 0) h.offset[s - 1] = layout.offset[s];
    }
    put(out, layout.offset[kFrames], frames);
    put(out, layout.offset[kClips], t.clips);
    put(out, layout.offset[kClipFrames], t.clip_frames);
    put(out, layout.offset[kMarkers], t.markers);
    std::memcpy(out.data(), &h, sizeof h);
    h.crc = nnh::detail::sprite_file_crc(out.data(), out.size());
    std::memcpy(out.data(), &h, sizeof h);
    const nnh::detail::SpriteCheck check = nnh::detail::validate_sprite_library(out.data(), out.size());
    if (!check.ok()) {
        return fail(problem, file, 0, std::string("internal error: the written library fails the engine's check (") +
                                          check.reason + ")");
    }
    return true;
}

} // namespace nnh::assetc
