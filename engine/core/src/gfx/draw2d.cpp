// 2D draw calls, shapes and cameras (nnh/gfx.hpp): they record into the frame's draw list
// (docs/architecture/rendering.md); gfx_frame.cpp submits it at the end of the frame.

#include "core/engine_state.hpp"
#include "gfx/gfx_state.hpp"
#include "log/engine_channels.hpp"
#include "nnh/assert.hpp"
#include "nnh/gfx.hpp"
#include "nnh/log.hpp"
#include "text/font_texture.hpp"

namespace nnh {
namespace {

using Kind = platform::DrawItem2D::Kind;

int screen_index(Screen screen) { return screen == Screen::Top ? 0 : 1; }
std::uint8_t clamp_layer(int layer) { return detail::gfx_clamp_layer(layer); }
void record(const detail::DrawCommand& command) { detail::gfx_record(command); }
bool drawing() { return detail::gfx_drawing(); }

detail::DrawCommand shape(Kind kind, Screen screen, Vec2 a, Vec2 b, Color color, const ShapeOptions& options) {
    detail::DrawCommand c;
    c.kind = kind;
    c.screen = screen;
    c.layer = clamp_layer(options.layer);
    c.ignore_camera = options.ignore_camera;
    c.position = a;
    c.second = b;
    c.color = color;
    return c;
}

} // namespace

bool detail::gfx_drawing() { return frame_open() && gfx_state().ready; }

void detail::gfx_record(const DrawCommand& command) {
    GfxState& g = gfx_state();
    if (!g.draws.record(command)) {
        // Release: dropped and logged once per frame (gfx_submit).
        NNH_ASSERT(false, "more than %d 2D draws in a frame: raise AppConfig::gfx.max_draws", g.max_draws);
    }
}

bool detail::gfx_tiles_enabled() {
    GfxState& g = gfx_state();
    if (g.max_tiles > 0) return true;
    if (!g.warned_tiles_off) NNH_LOG_WARN(gfx, "draw_tilemap ignored: AppConfig::gfx.max_tiles is 0");
    g.warned_tiles_off = true;
    NNH_ASSERT(false, "draw_tilemap needs AppConfig::gfx.max_tiles > 0, e.g. .gfx = {.max_tiles = 2048}");
    return false;
}

void detail::gfx_record_tiles(Screen screen, int layer, const TileBatchRecord& batch) {
    GfxState& g = gfx_state();
    if (!gfx_tiles_enabled()) return;
    TileBatchRecord stored = batch;
    stored.layer = clamp_layer(layer);
    const int index = g.tiles.add(stored);
    if (index < 0) {
        // Release: dropped (a whole layer goes missing, so it is logged every time).
        NNH_LOG_WARN(gfx, "more than %d tile batches in a frame: one dropped", kMaxTileBatches);
        NNH_ASSERT(false, "more than %d tile batches in a frame (one per map layer and tileset)",
                   kMaxTileBatches);
        return;
    }
    DrawCommand c;
    c.screen = screen;
    c.layer = stored.layer;
    c.tile_batch = static_cast<std::int16_t>(index);
    record(c);
}

void detail::gfx_forget_tiles(const void* source) {
    gfx_state().tiles.forget(source);
}

std::uint8_t detail::gfx_clamp_layer(int layer) {
    if (layer < 0) return 0;
    return static_cast<std::uint8_t>(layer >= kLayerCount ? kLayerCount - 1 : layer);
}

void gfx::draw(Screen screen, const Texture& texture, Vec2 position, const DrawOptions& options) {
    if (!drawing() || !texture.valid()) return;
    detail::DrawCommand c;
    c.kind = Kind::Sprite;
    c.screen = screen;
    c.layer = clamp_layer(options.layer);
    c.ignore_camera = options.ignore_camera;
    c.flip_x = options.flip_x;
    c.flip_y = options.flip_y;
    c.texture_slot = TextureAccess::slot(texture);
    c.texture_generation = TextureAccess::generation(texture);
    c.region = options.region;
    c.position = position;
    c.pivot = options.pivot;
    c.scale = options.scale;
    c.rotation = options.rotation;
    const float opacity = options.opacity < 0 ? 0 : (options.opacity > 1 ? 1 : options.opacity);
    c.color = options.tint;
    c.color.a = static_cast<std::uint8_t>(static_cast<float>(options.tint.a) * opacity + 0.5f);
    record(c);
}

void gfx::fill_rect(Screen screen, Rect rect, Color color, const ShapeOptions& options) {
    if (drawing()) record(shape(Kind::FillRect, screen, {rect.x, rect.y}, {rect.w, rect.h}, color, options));
}

void gfx::draw_rect(Screen screen, Rect rect, Color color, const ShapeOptions& options) {
    if (drawing()) record(shape(Kind::RectOutline, screen, {rect.x, rect.y}, {rect.w, rect.h}, color, options));
}

void gfx::draw_line(Screen screen, Vec2 from, Vec2 to, Color color, const ShapeOptions& options) {
    if (drawing()) record(shape(Kind::Line, screen, from, to, color, options));
}

void gfx::set_camera(Screen screen, Camera2D camera) {
    const float z = camera.zoom;
    if (!(z >= kMinCameraZoom && z <= kMaxCameraZoom)) { // NaN too
        NNH_ASSERT(false, "Camera2D::zoom %g is outside 0.5..4", static_cast<double>(z));
        camera.zoom = z > kMaxCameraZoom ? kMaxCameraZoom : (z < kMinCameraZoom ? kMinCameraZoom : 1.0f);
    }
    detail::gfx_cameras()[screen_index(screen)] = camera;
}

Camera2D gfx::camera(Screen screen) { return detail::gfx_cameras()[screen_index(screen)]; }

gfx::Stats gfx::stats() {
    const detail::GfxState& g = detail::gfx_state();
    Stats s;
    s.draws_last_frame = g.draws_last_frame;
    s.max_draws = g.max_draws;
    s.dropped_draws = g.dropped_total;
    // The game's textures only: the built-in font has its own slot and memory tag.
    const int font = detail::font_texture_ready() ? 1 : 0;
    s.textures = g.textures.active_count() - font;
    s.max_textures = g.textures.capacity() > 0 ? g.textures.capacity() - 1 : 0;
    s.texture_bytes = g.textures.total_bytes() - detail::font_texture_bytes();
    s.tiles_last_frame = g.tiles_last_frame;
    s.max_tiles = g.max_tiles;
    s.dropped_tiles = g.dropped_tiles_total;
    if (detail::g_gfx_particle_ops != nullptr) detail::g_gfx_particle_ops->stats(s);
    return s;
}

} // namespace nnh
