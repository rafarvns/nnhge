// Animated tiles: current frames and the expansion's table (see tile_animation.hpp).

#include "tilemap/tile_animation.hpp"

namespace nnh::detail {

using namespace mapfmt;

int animation_frame_index(const FrameEntry* frames, int count, std::uint64_t time_ms) {
    std::uint32_t total = 0; // ≤ 32 × 65535: no overflow
    for (int i = 0; i < count; ++i) total += frames[i].ms;
    std::uint32_t t = static_cast<std::uint32_t>(time_ms % total);
    int i = 0;
    while (t >= frames[i].ms) {
        t -= frames[i].ms;
        ++i;
    }
    return i;
}

int find_animation(const AnimationEntry* animations, int count, int tile_id) {
    int low = 0, high = count; // [low, high) may hold it
    while (low < high) {
        const int mid = (low + high) / 2;
        const int id = animations[mid].tile_id;
        if (id == tile_id) return mid;
        if (id < tile_id) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }
    return -1;
}

AnimationRange animations_in(const AnimationEntry* animations, int count, int first_id, int tile_count) {
    int first = 0;
    while (first < count && animations[first].tile_id < first_id) ++first;
    int end = first;
    while (end < count && animations[end].tile_id < first_id + tile_count) ++end;
    return {first, end - first};
}

void prepare_animations(const MapView& v, int tileset, std::uint64_t time_ms, TileSource* table, ExpandInput& in) {
    const Header& h = v.header();
    in.flags = nullptr;
    in.animations = nullptr;
    in.animation_count = 0;
    in.animated = nullptr;
    if (h.animations == 0) return;
    const TilesetEntry& set = v.tilesets()[tileset];
    const AnimationEntry* all = v.section<AnimationEntry>(kAnimations);
    const AnimationRange range = animations_in(all, h.animations, set.first_id, set.tile_count);
    if (range.count == 0) return;
    const FrameEntry* frames = v.section<FrameEntry>(kFrames);
    const TileSource* sources = v.tile_sources();
    for (int i = 0; i < range.count; ++i) {
        const AnimationEntry& a = all[range.first + i];
        const FrameEntry* own = frames + a.first_frame;
        table[i] = sources[own[animation_frame_index(own, a.frame_count, time_ms)].tile_id];
    }
    in.flags = v.tile_flags();
    in.animations = all + range.first;
    in.animation_count = range.count;
    in.animated = table;
}

} // namespace nnh::detail
