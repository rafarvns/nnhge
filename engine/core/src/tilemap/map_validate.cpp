// Bounded validation of a `.nnhmap` file (see map_validate.hpp): header, layout, strings, tilesets and the tile
// table. Layers, objects and properties: map_validate_content.cpp.

#include "tilemap/map_validate.hpp"

#include "core/crc32.hpp"
#include "tilemap/map_layout.hpp"
#include "tilemap/map_validate_parts.hpp"

namespace nnh::detail {

namespace mapcheck {

int Reader::tileset_of(int id) const {
    int s = 0;
    while (s + 1 < h.tilesets && at<mapfmt::TilesetEntry>(mapfmt::kTilesets, s + 1).first_id <= id) ++s;
    return s;
}

} // namespace mapcheck

namespace {

using namespace mapfmt;
using namespace mapcheck;

constexpr std::size_t kCrcStart = 16; // magic, version, size and the CRC itself are not covered

bool counts_fit(const Header& h) {
    return h.tilesets <= h.tilesets_cap && h.tile_layers <= h.tile_layers_cap &&
           h.object_layers <= h.object_layers_cap && h.tile_ids <= h.tile_ids_cap && h.tile_props <= h.tile_props_cap &&
           h.animations <= h.animations_cap && h.frames <= h.frames_cap && h.objects <= h.objects_cap &&
           h.properties <= h.properties_cap && h.string_bytes <= h.string_cap;
}

MapCheck check_header(const std::uint8_t* bytes, std::size_t size, Header& h) {
    if (bytes == nullptr || size < sizeof(Header)) return damaged("too short to be a map");
    std::memcpy(&h, bytes, sizeof h);
    if (std::memcmp(h.magic, kMagic, sizeof h.magic) != 0) return damaged("not a map file (no NNHM magic)");
    if (h.version > kVersion) return newer("made by a newer engine (format version)");
    if (h.version != kVersion) return damaged("unknown format version");
    if (h.file_size != size) return damaged("file size does not match its header (truncated?)");
    if (crc32(bytes + kCrcStart, size - kCrcStart) != h.crc) return damaged("CRC mismatch: the file is damaged");
    if (h.tile_width < kMinTileSize || h.tile_width > kMaxTileSize || h.tile_height < kMinTileSize ||
        h.tile_height > kMaxTileSize) {
        return over("tile size outside 8..64 pixels");
    }
    const MapCapacities caps = capacities_of(h);
    const CapacityCheck limits = check_capacities(caps);
    if (!limits.ok) return limits.limit ? over(limits.reason) : damaged(limits.reason);
    const MapLayout layout = map_layout(caps);
    if (layout.total != size) return damaged("block size does not match the capacities");
    for (int s = 0; s < kSectionCount; ++s) {
        if (layout.offsets[s] != h.offsets[s]) return damaged("section offsets do not match the capacities");
    }
    if (!counts_fit(h)) return damaged("a count is over its capacity");
    if (!std::isfinite(h.parallax_origin_x) || !std::isfinite(h.parallax_origin_y)) {
        return damaged("parallax origin not finite");
    }
    return {};
}

MapCheck check_strings(const Reader& r) {
    const char* pool = r.pool();
    const std::uint32_t used = r.h.string_bytes;
    if (used == 0 || pool[0] != '\0') return damaged("string pool does not start with the empty string");
    if (pool[used - 1] != '\0') return damaged("last string is not terminated");
    int run = 0;
    for (std::uint32_t i = 1; i < used; ++i) {
        run = pool[i] == '\0' ? 0 : run + 1;
        if (run > kMaxStringLength) return damaged("string over 255 bytes");
    }
    return {};
}

bool texture_side(int n) { return n >= 8 && n <= 1024 && (n & (n - 1)) == 0; }

MapCheck check_tilesets(const Reader& r) {
    const Header& h = r.h;
    int next = 1;
    for (int i = 0; i < h.tilesets; ++i) {
        const TilesetEntry s = r.at<TilesetEntry>(kTilesets, i);
        if (s.first_id != next || s.tile_count == 0) return damaged("tileset ids do not follow each other");
        next += s.tile_count;
        if (!r.string_ok(s.name)) return damaged("tileset name: string offset outside the pool");
        if (!texture_side(s.image_width) || !texture_side(s.image_height)) {
            return damaged("tileset image size is not 2^n from 8 to 1024");
        }
        const int columns = tiles_along(s.image_width, h.tile_width, s.margin, s.spacing);
        const int rows = tiles_along(s.image_height, h.tile_height, s.margin, s.spacing);
        if (s.columns != columns) return damaged("tileset columns do not match its image");
        if (s.tile_count > columns * rows) return damaged("tileset tiles do not fit its image");
        for (int t = 0; t < s.tile_count; ++t) {
            const TileSource want = tile_source(s, t, h.tile_width, h.tile_height);
            const TileSource got = r.at<TileSource>(kTileSources, s.first_id + t);
            if (got.x != want.x || got.y != want.y) return damaged("tile source does not match its tileset");
        }
    }
    if (next - 1 != h.tile_ids) return damaged("tileset ids do not add up to the header's tile ids");
    const TileSource none = r.at<TileSource>(kTileSources, 0);
    if (none.x != 0 || none.y != 0) return damaged("tile source of id 0 (empty) is not zero");
    return {};
}

/// Animations sorted by tile id, each on an animated tile, owning the next run of 1..32 frames; every frame a tile
/// of the animated tile's tileset, shown 1 ms or more.
MapCheck check_animations(const Reader& r, int animated_flags) {
    const Header& h = r.h;
    if (animated_flags != h.animations) return damaged("animated flags do not match the animation table");
    int previous = 0;
    int next_frame = 0;
    for (int i = 0; i < h.animations; ++i) {
        const AnimationEntry a = r.at<AnimationEntry>(kAnimations, i);
        if (a.tile_id < 1 || a.tile_id > h.tile_ids) return damaged("animation of an unknown tile id");
        if (a.tile_id <= previous) return damaged("animation table not sorted by tile id");
        previous = a.tile_id;
        if ((r.at<std::uint8_t>(kTileFlags, a.tile_id) & kTileAnimated) == 0) {
            return damaged("animated flags do not match the animation table");
        }
        if (a.frame_count == 0) return damaged("animation without frames");
        if (a.frame_count > kMaxFramesPerAnimation) return over("more than 32 frames in one animation");
        if (a.first_frame != next_frame || next_frame + a.frame_count > h.frames) {
            return damaged("animation frames do not follow each other");
        }
        const int set = r.tileset_of(a.tile_id);
        for (int f = next_frame; f < next_frame + a.frame_count; ++f) {
            const FrameEntry frame = r.at<FrameEntry>(kFrames, f);
            if (frame.tile_id < 1 || frame.tile_id > h.tile_ids || r.tileset_of(frame.tile_id) != set) {
                return damaged("animation frame is not a tile of the animated tile's tileset");
            }
            if (frame.ms == 0) return damaged("animation frame of 0 ms");
        }
        next_frame += a.frame_count;
    }
    if (next_frame != h.frames) return damaged("animation frames do not add up to the header's frames");
    return {};
}

MapCheck check_tile_flags(const Reader& r) {
    const Header& h = r.h;
    if (r.at<std::uint8_t>(kTileFlags, 0) != 0) return damaged("tile flags on id 0 (empty)");
    int with_properties = 0;
    int animated = 0;
    for (int id = 1; id <= h.tile_ids; ++id) {
        const std::uint8_t f = r.at<std::uint8_t>(kTileFlags, id);
        if ((f & ~(kTileCollisionMask | kTileAnimated | kTileHasProperties)) != 0 || (f & kTileCollisionMask) == 3) {
            return damaged("unknown tile flags");
        }
        if ((f & kTileHasProperties) != 0) ++with_properties;
        if ((f & kTileAnimated) != 0) ++animated;
    }
    const MapCheck animations = check_animations(r, animated);
    if (!animations.ok()) return animations;
    int previous = 0;
    for (int i = 0; i < h.tile_props; ++i) {
        const int id = r.at<TilePropsEntry>(kTileProps, i).tile_id;
        if (id < 1 || id > h.tile_ids) return damaged("tile property entry for an unknown tile id");
        if (id <= previous) return damaged("tile property table not sorted by tile id");
        if ((r.at<std::uint8_t>(kTileFlags, id) & kTileHasProperties) == 0) {
            return damaged("tile has-properties flags do not match the tile property table");
        }
        previous = id;
    }
    if (with_properties != h.tile_props) return damaged("tile has-properties flags do not match the tile property table");
    return {};
}

} // namespace

MapCheck validate_map(const void* data, std::size_t size) {
    Reader r;
    r.data = static_cast<const std::uint8_t*>(data);
    MapCheck c = check_header(r.data, size, r.h);
    if (c.ok()) c = check_strings(r);
    if (c.ok()) c = check_tilesets(r);
    if (c.ok()) c = check_tile_flags(r);
    if (c.ok()) c = check_tile_layers(r);
    if (c.ok()) c = check_objects(r);
    if (c.ok()) c = check_properties(r);
    return c;
}

} // namespace nnh::detail
