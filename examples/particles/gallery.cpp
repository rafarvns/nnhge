#include "gallery.hpp"

#include <nnh/gfx.hpp>
#include <nnh/transform.hpp>

#include "demos.hpp"
#include "effects.hpp"
#include "presets.hpp"

namespace {

constexpr nnh::Screen kTop = nnh::Screen::Top;
constexpr nnh::Vec2 kTorchAt{48, 196}, kChimneyAt{128, 150}, kExplosionAt{300, 80}, kFountainAt{360, 222},
    kEmbersAt{220, 222};

bool make(nnh::ParticleEmitter& out, const nnh::ParticleDesc& d, int capacity) {
    auto e = nnh::ParticleEmitter::create(d, capacity);
    if (!e) {
        NNH_LOG_ERROR(example, "emitter: %s", e.error().message);
        return false;
    }
    out = static_cast<nnh::ParticleEmitter&&>(e.value());
    return true;
}

} // namespace

const char* Gallery::name(int i) {
    static const char* const kNames[kEmitters] = {"torch", "smoke", "rain", "exhaust", "explosion", "fountain", "embers"};
    return kNames[i];
}

bool Gallery::create() {
    auto tex = nnh::load_texture("art/particles");
    if (!tex) {
        NNH_LOG_ERROR(example, "art/particles: %s", tex.error().message);
        return false;
    }
    sheet_ = static_cast<nnh::Texture&&>(tex.value());
    auto map = nnh::Tilemap::create({.width = 25, .height = 15, .max_tile_layers = 1, .max_tilesets = 1});
    if (!map) return false;
    band_ = static_cast<nnh::Tilemap&&>(map.value());
    const auto first = band_.add_tileset({.texture = &sheet_});
    const auto layer = band_.add_tile_layer({.name = "band"});
    if (!first || !layer) return false;
    band_.fill(layer.value(), 0, 8, 25, 1, {.id = static_cast<std::uint16_t>(first.value() + 15)}); // the brick tile
    nnh::ParticleDesc embers;
    if (!load_preset("embers", embers)) return false;
    const nnh::ParticleDesc descs[kEmitters] = {effects::torch(),     effects::smoke(),    effects::rain(),
                                                effects::exhaust(),   effects::explosion(), effects::fountain(),
                                                embers};
    const int capacity[kEmitters] = {128, 128, 256, 256, 256, 128, 160};
    for (int i = 0; i < kEmitters; ++i) {
        if (!make(emitters_[i], descs[i], capacity[i])) return false;
    }
    emitters_[kTorch].set_position(kTorchAt, true);
    emitters_[kSmoke].set_position(kChimneyAt, true);
    emitters_[kRain].set_position({200, -12}, true);
    emitters_[kExplosion].set_position(kExplosionAt, true);
    emitters_[kFountain].set_position(kFountainAt, true);
    emitters_[kEmbers].set_position(kEmbersAt, true);
    emitters_[kExplosion].stop(); // A fires it
    return true;
}

void Gallery::update(nnh::Vec2 pad, int frame) {
    // The ship: the Circle Pad, or a figure of eight when it is left alone.
    if (pad.x != 0 || pad.y != 0) {
        ship_ = {ship_.x + pad.x * 3, ship_.y - pad.y * 3};
    } else {
        const nnh::SinCos a = nnh::sin_cos(static_cast<float>(frame) * 0.03f);
        const nnh::SinCos b = nnh::sin_cos(static_cast<float>(frame) * 0.06f);
        ship_ = {200 + 150 * a.sin, 70 + 40 * b.sin};
        ship_angle_ = a.cos * 0.6f;
    }
    ship_.x = ship_.x < 10 ? 10 : (ship_.x > 390 ? 390 : ship_.x);
    ship_.y = ship_.y < 10 ? 10 : (ship_.y > 230 ? 230 : ship_.y);
    emitters_[kExhaust].set_transform(nnh::Transform2D{.position = ship_, .rotation = ship_angle_});
    const float sway = nnh::sin_cos(static_cast<float>(frame) * 0.05f).sin * 0.35f;
    emitters_[kTorch].set_transform(
        nnh::Transform2D{.position = {kTorchAt.x + sway * 20, kTorchAt.y}, .rotation = sway});
    for (nnh::ParticleEmitter& e : emitters_) e.update();
}

void Gallery::draw() const {
    namespace gfx = nnh::gfx;
    gfx::draw_tilemap(kTop, band_, {.layer = kBandLayer});
    gfx::draw(kTop, sheet_, {kChimneyAt.x - 8, kChimneyAt.y}, {.region = effects::kChimney, .layer = 3});
    gfx::fill_rect(kTop, {kTorchAt.x - 2, kTorchAt.y, 4, 20}, nnh::rgb(110, 70, 40), {.layer = 3});
    gfx::fill_rect(kTop, {kEmbersAt.x - 14, kEmbersAt.y, 28, 6}, nnh::rgb(70, 60, 60), {.layer = 3});
    for (int i = 0; i < kEmitters; ++i) {
        if (i == kFountain) {
            emitters_[i].draw(kTop, {.layer = 4}); // texture-less squares
        } else if (i == kRain) {
            emitters_[i].draw(kTop, sheet_, {.layer = rain_front_ ? kBandLayer + 1 : kBandLayer - 1});
        } else {
            emitters_[i].draw(kTop, sheet_, {.layer = 4});
        }
    }
    gfx::draw(kTop, sheet_, ship_, {.region = effects::kShip, .pivot = {8, 8}, .rotation = ship_angle_, .layer = 7});
}

void Gallery::explode() { emitters_[kExplosion].restart(); }

void Gallery::set_local(bool local) {
    for (int i : {static_cast<int>(kTorch), static_cast<int>(kExhaust)}) {
        nnh::ParticleDesc d = emitters_[i].desc();
        d.space = local ? nnh::ParticleSpace::Local : nnh::ParticleSpace::World;
        emitters_[i].set_desc(d);
        emitters_[i].restart(); // the stored positions change meaning with the space
    }
}

void Gallery::toggle_smoke() {
    if (emitters_[kSmoke].emitting()) {
        emitters_[kSmoke].stop();
    } else {
        emitters_[kSmoke].start();
    }
}

int Gallery::alive() const {
    int n = 0;
    for (const nnh::ParticleEmitter& e : emitters_) n += e.alive();
    return n;
}
