#include "touch_panel.hpp"

#include <nnh/input.hpp>

#include "demos.hpp"
#include "effects.hpp"
#include "presets.hpp"

namespace {

constexpr int kCounts[TouchPanel::kEffects] = {16, 10, 40};

} // namespace

const char* TouchPanel::name(int i) {
    static const char* const kNames[kEffects] = {"sparks", "smoke", "fireworks"};
    return kNames[i];
}

bool TouchPanel::create() {
    nnh::ParticleDesc sparks;
    if (!load_preset("sparks", sparks)) return false;
    nnh::ParticleDesc smoke = effects::smoke();
    smoke.rate = 0;
    const nnh::ParticleDesc descs[kEffects] = {sparks, smoke, effects::fireworks()};
    for (int i = 0; i < kEffects; ++i) {
        auto e = nnh::ParticleEmitter::create(descs[i], 256);
        if (!e) return false;
        emitters_[i] = static_cast<nnh::ParticleEmitter&&>(e.value());
    }
    return true;
}

void TouchPanel::burst(int x, int y) {
    nnh::ParticleEmitter& e = emitters_[selected_];
    const int made = e.emit_at(nnh::Vec2{static_cast<float>(x), static_cast<float>(y)}, kCounts[selected_]);
    waiting_[selected_] = true;
    NNH_LOG_INFO(example, "burst: %s %d at %d,%d live %d", name(selected_), made, x, y, e.alive());
}

void TouchPanel::overfill() {
    nnh::ParticleEmitter& e = emitters_[selected_];
    e.emit_at(nnh::Vec2{160, 120}, 400);
    waiting_[selected_] = true;
    NNH_LOG_INFO(example, "full: %s live %d dropped %u", name(selected_), e.alive(),
                 static_cast<unsigned>(e.stats().dropped));
}

void TouchPanel::update() {
    const bool next = nnh::input::pressed(nnh::Button::R), previous = nnh::input::pressed(nnh::Button::L);
    if (next) selected_ = (selected_ + 1) % kEffects;
    if (previous) selected_ = (selected_ + kEffects - 1) % kEffects;
    if (next || previous) NNH_LOG_INFO(example, "effect: %s", name(selected_));
    if (nnh::input::touch_started()) {
        const nnh::TouchPoint t = nnh::input::touch_position();
        burst(t.x, t.y);
    }
    for (int i = 0; i < kEffects; ++i) {
        emitters_[i].update();
        if (waiting_[i] && emitters_[i].finished()) {
            waiting_[i] = false;
            NNH_LOG_INFO(example, "done: %s live 0 finished", name(i));
        }
    }
}

void TouchPanel::draw(const nnh::Texture& sheet) const {
    for (const nnh::ParticleEmitter& e : emitters_) e.draw(nnh::Screen::Bottom, sheet, {.layer = 2});
}
