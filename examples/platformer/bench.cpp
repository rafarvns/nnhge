#include "bench.hpp"

#include <new>
#include <nnh/debug.hpp>
#include <nnh/gfx.hpp>
#include <nnh/input.hpp>
#include <nnh/time.hpp>
#include <utility>

namespace {

constexpr int kMapW = 48; // cells: 768 x 384 px, room to scroll
constexpr int kMapH = 24;
constexpr float kScreenW = 400 - 16;
constexpr float kScreenH = 240 - 16;
constexpr double kMissedMs = 1.5 * 1000.0 / 60.0; // a frame that missed its 60 Hz slot

// Mostly transparent tiles, so every layer shows through the ones above it.
constexpr std::uint16_t kOverlay[] = {tile::Lattice, tile::Star,    tile::CloudMiddle, tile::Bush,
                                      tile::Flower,  tile::Tuft,    tile::Plank,       tile::Coin,
                                      tile::Water,   tile::HillTop, tile::CloudLeft,   tile::CloudRight};

/// Small deterministic generator, so every run starts the same.
float random01(std::uint32_t& state) {
    state = state * 1664525u + 1013904223u;
    return static_cast<float>(state >> 8) / 16777216.0f;
}

/// Bounces `v` between `lo` and `hi`.
void bounce(float& v, float& speed, float lo, float hi) {
    v += speed;
    if (v < lo || v > hi) speed = -speed;
}

} // namespace

Bench::Bench(const Art& art) : art_(art) {
    auto made = nnh::Tilemap::create({.width = kMapW,
                                      .height = kMapH,
                                      .max_tile_layers = kMaxLayers,
                                      .max_tilesets = 1,
                                      .max_tile_ids = tile::kCount,
                                      .max_object_layers = 0,
                                      .max_objects = 0,
                                      .max_properties = 0,
                                      .string_bytes = 64});
    if (made) map_ = std::move(made.value());
    if (!map_.valid() || !map_.add_tileset({.texture = &art.tiles, .tile_count = tile::kCount})) {
        map_.reset();
        return;
    }
    for (int l = 0; l < kMaxLayers; ++l) {
        if (!map_.add_tile_layer({.offset = {static_cast<float>(l * 5), static_cast<float>(l * 3)}})) break;
        const std::uint16_t a = l == 0 ? tile::DarkBricks : kOverlay[(l * 2) % 12];
        const std::uint16_t b = l == 0 ? tile::DarkBricks : kOverlay[(l * 2 + 1) % 12];
        for (int y = 0; y < kMapH; ++y) {
            for (int x = 0; x < kMapW; ++x) {
                map_.set_tile(l, x, y, {.id = (x + y) % 2 == 0 ? a : b, .flip_x = x % 3 == 0});
            }
        }
    }
    sprites_.reset(new (std::nothrow) Sprite[kMaxSprites]);
    if (sprites_ == nullptr) return;
    std::uint32_t seed = 12345;
    for (int i = 0; i < kMaxSprites; ++i) {
        sprites_[i].x = random01(seed) * kScreenW;
        sprites_[i].y = random01(seed) * kScreenH;
        sprites_[i].vx = (random01(seed) - 0.5f) * 4;
        sprites_[i].vy = (random01(seed) - 0.5f) * 4;
    }
}

void Bench::set_counts(int layers, int sprites) {
    layer_count_ = layers < 0 ? 0 : (layers > kMaxLayers ? kMaxLayers : layers);
    sprite_count_ = sprites < 0 ? 0 : (sprites > kMaxSprites ? kMaxSprites : sprites);
    average_ms_ = worst_ms_ = 0;
    frames_ = missed_ = 0;
}

void Bench::start_gate() {
    gate_.start();
    show_verdict_ = false;
    set_counts(gate_.counts().layers, gate_.counts().sprites);
}

void Bench::manual_controls() {
    using nnh::Button;
    namespace input = nnh::input;
    if (input::pressed(Button::A)) {
        start_gate();
        return;
    }
    const int layers =
        layer_count_ + (input::pressed(Button::R) ? 1 : 0) - (input::pressed(Button::L) ? 1 : 0);
    const int sprites =
        sprite_count_ + (input::pressed(Button::X) ? 100 : 0) - (input::pressed(Button::Y) ? 100 : 0);
    if (layers != layer_count_ || sprites != sprite_count_) {
        set_counts(layers, sprites);
        show_verdict_ = false;
    }
}

void Bench::update() {
    const double ms = nnh::time::delta().seconds() * 1000.0;
    if (gate_.running()) {
        const GateCounts c = gate_.frame(ms);
        if (!gate_.running()) show_verdict_ = true; // finished this frame: back to the manual bench
        if (c.layers != layer_count_ || c.sprites != sprite_count_) set_counts(c.layers, c.sprites);
    } else {
        manual_controls();
    }
    // The camera wanders over the map at sub-pixel speeds: layers are almost never cell-aligned (worst case).
    bounce(camera_.x, camera_speed_.x, 16, kMapW * 16 - 400 - 80);
    bounce(camera_.y, camera_speed_.y, 16, kMapH * 16 - 240 - 48);
    nnh::gfx::set_camera(nnh::Screen::Top, {camera_});
    for (int i = 0; i < sprite_count_; ++i) {
        bounce(sprites_[i].x, sprites_[i].vx, 0, kScreenW);
        bounce(sprites_[i].y, sprites_[i].vy, 0, kScreenH);
    }
    if (++frames_ > 1) { // the first frame after a change still carries the previous load
        average_ms_ = average_ms_ == 0 ? ms : average_ms_ * 0.95 + ms * 0.05;
        if (ms > worst_ms_) worst_ms_ = ms;
        if (ms > kMissedMs) ++missed_;
    }
}

void Bench::draw() {
    namespace gfx = nnh::gfx;
    const nnh::Screen top = nnh::Screen::Top;
    // Map layers behind the sprites, the last one in front of them (like a level's foreground).
    for (int l = 0; l < layer_count_; ++l)
        gfx::draw_tilemap(top, map_, {.map_layer = l, .layer = l + 1 < layer_count_ ? 0 : 2});
    for (int i = 0; i < sprite_count_; ++i) {
        gfx::draw(top, art_.player, {sprites_[i].x, sprites_[i].y},
                  {.region = {(i % 4) * 16, 0, 16, 16}, .layer = 1, .ignore_camera = true});
    }
    if (show_verdict_) gate_.draw_banner();
    if (gate_.running() || show_verdict_)
        gate_.draw_panel();
    else
        draw_stats();
}

void Bench::draw_stats() const {
    namespace gfx = nnh::gfx;
    const nnh::Screen bottom = nnh::Screen::Bottom;
    const int line = nnh::debug::line_height();
    const gfx::Stats s = gfx::stats();
    nnh::debug::printf(bottom, 12, 12, "TILE BENCHMARK (spec 012 gate)");
    nnh::debug::printf(bottom, 12, 12 + 2 * line, "layers  %2d  (16x16, full)   L/R", layer_count_);
    nnh::debug::printf(bottom, 12, 12 + 3 * line, "sprites %4d                Y/X", sprite_count_);
    nnh::debug::printf(bottom, 12, 12 + 5 * line, "frame   %5.1f ms  (%.0f FPS)", average_ms_,
                       average_ms_ > 0 ? 1000.0 / average_ms_ : 0.0);
    nnh::debug::printf(bottom, 12, 12 + 6 * line, "worst   %5.1f ms", worst_ms_);
    nnh::debug::printf(bottom, 12, 12 + 7 * line, "missed  %d of %d frames", missed_,
                       frames_ > 1 ? frames_ - 1 : 0);
    nnh::debug::printf(bottom, 12, 12 + 9 * line, "tiles   %d / %d  dropped %d", s.tiles_last_frame,
                       s.max_tiles, s.dropped_tiles);
    nnh::debug::printf(bottom, 12, 12 + 10 * line, "draws   %d / %d", s.draws_last_frame, s.max_draws);
    nnh::debug::print(bottom, 12, 12 + 12 * line, "Counts reset the frame stats.");
    nnh::debug::print(bottom, 12, 12 + 13 * line, "60 Hz = missed stays 0 for 10 s.");
    if (gate_.finished())
        nnh::debug::printf(bottom, 12, 12 + 15 * line, "A: automatic gate (last: %s)",
                           gate_.passed() ? "PASS" : "FAIL");
    else
        nnh::debug::print(bottom, 12, 12 + 15 * line, "A: automatic gate (about 2 min)");
    nnh::debug::print(bottom, 12, 12 + 16 * line, "SELECT: next demo   START: exit");
}
