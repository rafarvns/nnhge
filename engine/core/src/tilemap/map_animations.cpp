// Animated tiles of a map: set_tile_animation, tile_animation, animated_tile_id (spec 012 stage 6). The table is
// kept as the validator wants it: animations sorted by tile id, each owning the next run of frames.

#include <cstring>

#include "tilemap/tile_animation.hpp"
#include "tilemap/tilemap_internal.hpp"

namespace nnh {

namespace {

using namespace detail::mapfmt;

Error invalid(const char* what) { return detail::map_refuse(ErrorCode::InvalidArgument, what); }
Error full(const char* what) { return detail::map_refuse(ErrorCode::LimitReached, what); }

AnimationEntry* table(const detail::MapView& v) { return v.section<AnimationEntry>(kAnimations); }
FrameEntry* frame_table(const detail::MapView& v) { return v.section<FrameEntry>(kFrames); }

/// The tileset holding tile `id` (1..tile_ids), or -1.
int tileset_of(const detail::MapView& v, int id) {
    for (int s = 0; s < v.header().tilesets; ++s) {
        const TilesetEntry& set = v.tilesets()[s];
        if (id >= set.first_id && id < set.first_id + set.tile_count) return s;
    }
    return -1;
}

/// Where tile `id`'s animation is, or where it would go to keep the table sorted.
int position(const detail::MapView& v, int id) {
    int at = 0;
    while (at < v.header().animations && table(v)[at].tile_id < id) ++at;
    return at;
}

/// Replaces the run of `old` frames at `first` by `count` frames, moving the later frames and their animations.
void replace_frames(const detail::MapView& v, int later, int first, int old, const Tilemap::AnimationFrame* frames,
                    int count) {
    Header& h = v.header();
    FrameEntry* f = frame_table(v);
    const int delta = count - old;
    std::memmove(f + first + count, f + first + old, static_cast<std::size_t>(h.frames - first - old) * sizeof *f);
    for (int i = 0; i < count; ++i) f[first + i] = {frames[i].tile_id, frames[i].ms};
    if (delta < 0) std::memset(f + h.frames + delta, 0, static_cast<std::size_t>(-delta) * sizeof *f);
    for (int i = later; i < h.animations; ++i) {
        table(v)[i].first_frame = static_cast<std::uint16_t>(table(v)[i].first_frame + delta);
    }
    h.frames = static_cast<std::uint16_t>(h.frames + delta);
}

/// What is wrong with set_tile_animation's arguments; `what` is null when they are fine.
struct Refusal {
    ErrorCode code = ErrorCode::None;
    const char* what = nullptr;
};

Refusal check_frames(const detail::MapView& v, int tile_id, const Tilemap::AnimationFrame* frames, int count) {
    const Header& h = v.header();
    const auto bad = [](const char* what) { return Refusal{ErrorCode::InvalidArgument, what}; };
    if (tile_id < 1 || tile_id > h.tile_ids) return bad("set_tile_animation: unknown tile id");
    if (count < 0 || (count > 0 && frames == nullptr)) return bad("set_tile_animation: no frames");
    if (count > kMaxFramesPerAnimation) return {ErrorCode::LimitReached, "more than 32 frames in one animation"};
    const int set = tileset_of(v, tile_id);
    for (int i = 0; i < count; ++i) {
        const int id = frames[i].tile_id;
        if (tileset_of(v, id) != set) { // also unknown ids (no tileset: -1)
            return bad("animation frame is not a tile of the animated tile's tileset");
        }
        if (frames[i].ms == 0) return bad("animation frame of 0 ms");
    }
    return {};
}

} // namespace

Result<void> Tilemap::set_tile_animation(int tile_id, const AnimationFrame* frames, int count) {
    const detail::MapView v = detail::map_view(*this);
    if (v.base == nullptr) return invalid("set_tile_animation on an empty map");
    const Refusal refusal = check_frames(v, tile_id, frames, count);
    if (refusal.what != nullptr) return detail::map_refuse(refusal.code, refusal.what);
    Header& h = v.header();
    const int at = position(v, tile_id);
    const bool exists = at < h.animations && table(v)[at].tile_id == tile_id;
    const int old = exists ? table(v)[at].frame_count : 0;
    if (!exists && count == 0) return {};
    if (!exists && h.animations >= h.animations_cap) return full("max_animations reached");
    if (h.frames - old + count > h.frames_cap) return full("max_animation_frames reached");

    const int first = at < h.animations ? table(v)[at].first_frame : h.frames;
    replace_frames(v, exists ? at + 1 : at, first, old, frames, count);
    AnimationEntry* t = table(v);
    std::uint8_t& flag = v.tile_flags()[tile_id];
    if (count == 0) { // exists: remove its entry
        std::memmove(t + at, t + at + 1, static_cast<std::size_t>(h.animations - at - 1) * sizeof *t);
        h.animations = static_cast<std::uint16_t>(h.animations - 1);
        t[h.animations] = {};
        flag = static_cast<std::uint8_t>(flag & ~kTileAnimated);
        return {};
    }
    if (!exists) {
        std::memmove(t + at + 1, t + at, static_cast<std::size_t>(h.animations - at) * sizeof *t);
        h.animations = static_cast<std::uint16_t>(h.animations + 1);
    }
    t[at] = {};
    t[at].tile_id = static_cast<std::uint16_t>(tile_id);
    t[at].frame_count = static_cast<std::uint8_t>(count);
    t[at].first_frame = static_cast<std::uint16_t>(first);
    flag = static_cast<std::uint8_t>(flag | kTileAnimated);
    return {};
}

int Tilemap::tile_animation(int tile_id, AnimationFrame* frames, int capacity) const {
    const detail::MapView v = detail::map_view(*this);
    if (v.base == nullptr) return 0;
    const int a = detail::find_animation(table(v), v.header().animations, tile_id);
    if (a < 0) return 0;
    const AnimationEntry& e = table(v)[a];
    const FrameEntry* own = frame_table(v) + e.first_frame;
    for (int i = 0; i < e.frame_count && i < capacity; ++i) frames[i] = {own[i].tile_id, own[i].ms};
    return e.frame_count;
}

int Tilemap::animated_tile_id(int tile_id, Duration time) const {
    const detail::MapView v = detail::map_view(*this);
    if (v.base == nullptr) return tile_id;
    const int a = detail::find_animation(table(v), v.header().animations, tile_id);
    if (a < 0) return tile_id;
    const AnimationEntry& e = table(v)[a];
    const FrameEntry* own = frame_table(v) + e.first_frame;
    const std::uint64_t ms = time.ns < 0 ? 0 : static_cast<std::uint64_t>(time.ms());
    return own[detail::animation_frame_index(own, e.frame_count, ms)].tile_id;
}

} // namespace nnh
