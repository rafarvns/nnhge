#pragma once
// The top screen's gallery (spec 024 AC-4): a torch (local space on a swaying transform, flame frames over life),
// chimney smoke, rain behind or in front of a code-built tile band, a ship (Circle Pad, or a fixed path) leaving an
// exhaust trail, an explosion (A), a fountain of texture-less squares and embers from a data preset.

#include <nnh/particles.hpp>
#include <nnh/texture.hpp>
#include <nnh/tilemap.hpp>

class Gallery {
public:
    static constexpr int kEmitters = 7;
    static constexpr int kBandLayer = 5; ///< the tile band's draw layer: rain at 4 (behind) or 6 (in front)

    /// Loads the sheet, builds the tile band and the emitters. False (logged) when something is missing.
    bool create();
    /// One frame: the ship from `pad` (a zero pad follows its own path), the torch's sway, every emitter's update.
    void update(nnh::Vec2 pad, int frame);
    void draw() const;

    /// A: the explosion again. @return the particles it made in its first frame (after one update).
    void explode();
    /// X: world/local space for the torch and the ship's trail (restarted).
    void set_local(bool local);
    /// B: rain in front of the tile band.
    void set_rain_front(bool front) { rain_front_ = front; }
    /// D-Pad Down: the smoke stops (its puffs drift away) or starts.
    void toggle_smoke();
    bool smoke_on() const { return emitters_[kSmoke].emitting(); }

    const nnh::ParticleEmitter& emitter(int i) const { return emitters_[i]; }
    static const char* name(int i);
    const nnh::Texture& sheet() const { return sheet_; }
    int alive() const;

    enum Index { kTorch, kSmoke, kRain, kExhaust, kExplosion, kFountain, kEmbers };

private:
    nnh::Texture sheet_;
    nnh::Tilemap band_;
    nnh::ParticleEmitter emitters_[kEmitters];
    nnh::Vec2 ship_{60, 60};
    float ship_angle_ = 0;
    bool rain_front_ = false;
};
