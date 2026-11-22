// gfx::draw_tilemap (nnh/tilemap.hpp): one tile batch per (map layer, tileset), expanded at the end of the
// frame (spec 012 FR-3, decision D26). gfx sees a batch only as {expand_batch, the map's block}; this file is
// the tilemap side of that bridge.

#include "gfx/gfx_state.hpp"
#include "nnh/app.hpp"
#include "nnh/assert.hpp"
#include "tilemap/tile_animation.hpp"
#include "tilemap/tile_expand.hpp"
#include "tilemap/tilemap_internal.hpp"
#include "tilemap/visible_range.hpp"
#include "tilemap/zoom_edges.hpp"

#include <algorithm>

namespace nnh {
namespace {

using namespace detail::mapfmt;

constexpr int kTilesetBits = 8; // arg = layer << 8 | tileset

/// Called by gfx at submit: the visible quads of one (layer, tileset) of the map block `r.source`, read now.
detail::TileExpandResult expand_batch(const detail::TileExpandRequest& r) {
    const detail::MapView v{static_cast<std::uint8_t*>(const_cast<void*>(r.source))};
    const Header& h = v.header();
    const int layer = r.arg >> kTilesetBits;
    const int tileset = r.arg & ((1 << kTilesetBits) - 1);
    if (layer >= h.tile_layers || tileset >= h.tilesets) return {};
    const TileLayerEntry& e = v.tile_layers()[layer];
    const TilesetEntry& set = v.tilesets()[tileset];
    detail::LayerPlacement p;
    p.camera = r.camera;
    p.zoom = r.zoom;
    p.screen_size = {static_cast<float>(screen_width(r.screen)), static_cast<float>(screen_height(r.screen))};
    p.offset = Vec2{e.offset_x, e.offset_y} + r.offset;
    p.parallax = {e.parallax_x, e.parallax_y};
    p.parallax_origin = {h.parallax_origin_x, h.parallax_origin_y};
    detail::ExpandInput in;
    in.cells = v.cells(layer);
    in.map_width = h.width;
    in.sources = v.tile_sources();
    in.first_id = set.first_id;
    in.tile_count = set.tile_count;
    in.view = detail::visible_range(p, h.tile_width, h.tile_height, h.width, h.height);
    in.tile_w = h.tile_width;
    in.tile_h = h.tile_height;
    in.first_row = r.first_row;
    in.chunk = r.chunk;
    TileSource animated[kMaxAnimations]; // 1 KB of stack, only read for this tileset's animations
    detail::prepare_animations(v, tileset, r.clock, animated, in);
    const detail::ExpandResult x = detail::expand_tiles(in, r.out, r.capacity);
    const int zq = in.view.zoom_q;
    return {x.written, x.dropped, x.next_row, detail::zoomed_size(h.tile_width, zq), detail::zoomed_size(h.tile_height, zq)};
}

/// Records the batches of one map layer (nothing if hidden, empty or fully transparent).
void record_layer(Screen screen, const Tilemap& map, int layer, const gfx::TilemapDrawOptions& o, std::uint64_t ms) {
    const detail::MapView v = detail::map_view(map);
    const Header& h = v.header();
    const TileLayerEntry& e = v.tile_layers()[layer];
    if ((e.flags & kLayerVisible) == 0) return;
    const float opacity = std::clamp(o.opacity, 0.0f, 1.0f) * e.opacity;
    const auto alpha = static_cast<std::uint8_t>(255.0f * opacity + 0.5f);
    if (alpha == 0) return;
    for (int s = 0; s < h.tilesets; ++s) {
        if ((e.tilesets_used & (1u << s)) == 0) continue;
        if (!detail::tileset_texture_alive(map, s)) {
            NNH_ASSERT(false,
                       "tilemap: tileset %d's texture was destroyed: keep it alive while the map is drawn",
                       s);
            continue;
        }
        const TilesetEntry& set = v.tilesets()[s];
        detail::TileBatchRecord r;
        r.expand = expand_batch;
        r.source = v.base;
        r.arg = (layer << kTilesetBits) | s;
        r.offset = o.offset;
        r.clock = ms;
        r.texture_slot = set.texture_slot;
        r.texture_generation = set.texture_generation;
        r.tile_w = h.tile_width;
        r.tile_h = h.tile_height;
        r.color = rgba(255, 255, 255, alpha);
        r.ignore_camera = o.ignore_camera;
        detail::gfx_record_tiles(screen, o.layer, r);
    }
}

} // namespace

void gfx::draw_tilemap(Screen screen, const Tilemap& map, const TilemapDrawOptions& options) {
    if (!detail::gfx_drawing() || !map.valid() || !detail::gfx_tiles_enabled()) return;
    const int layers = map.tile_layer_count();
    const std::uint64_t ms = options.animation_time.ns < 0 ? detail::default_animation_ms(frame_count())
                                                           : static_cast<std::uint64_t>(options.animation_time.ms());
    if (options.map_layer == -1) {
        for (int l = 0; l < layers; ++l)
            record_layer(screen, map, l, options, ms);
        return;
    }
    const bool known = options.map_layer >= 0 && options.map_layer < layers;
    NNH_ASSERT(known, "draw_tilemap: no tile layer %d (%d layers)", options.map_layer, layers);
    if (known) record_layer(screen, map, options.map_layer, options, ms);
}

} // namespace nnh
