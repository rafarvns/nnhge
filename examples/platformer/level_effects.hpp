#pragma once
// The level's cosmetic particle effects (spec 024 AC-5): dust when the player lands and when it starts running, a puff
// where a turret's bullet stops on a wall or hits the player, a flash at a turret's muzzle when it fires (spec 022's
// barrel). Three emitters outside the level's world: update() reads the player, the bullets, the turrets and the
// barrels' stored world transforms and never writes them, so the replay's sample (and its checksum lines) can't change.
// Texture-less squares, drawn on the player's draw layer. Pure but for draw(): tested in
// tests/platformer/test_level_effects.cpp.

#include <nnh/hierarchy.hpp>
#include <nnh/particles.hpp>

#include "player.hpp"
#include "turrets.hpp"

/// What the effects read each frame (after the player, the turrets and the hierarchy's pass).
struct EffectsInput {
    const Player& player;
    TurretBook& turrets;   ///< read only: bullets, hits
    nnh::World& world;     ///< read only: turrets and their barrels
    const nnh::Hierarchy& tree;
};

class LevelEffects {
public:
    static constexpr int kLandingDust = 10;
    static constexpr int kRunDust = 6;
    static constexpr int kWallPuff = 8;
    static constexpr int kHitPuff = 12;
    static constexpr int kMuzzleFlash = 6;
    static constexpr float kRunSpeed = 0.5f; ///< px per frame: crossing it on the ground starts a run

    LevelEffects();
    /// Emits for this frame's triggers, then updates the three emitters.
    void update(const EffectsInput& in);
    /// Every particle gone, triggers forgotten (a restart).
    void clear();
    /// The three emitters on the top screen at `layer`.
    void draw(int layer) const;

    const nnh::ParticleEmitter& dust() const { return dust_; }
    const nnh::ParticleEmitter& puff() const { return puff_; }
    const nnh::ParticleEmitter& flash() const { return flash_; }

private:
    void emit_player(const Player& p);
    void emit_bullets(TurretBook& turrets, const Player& p);
    void emit_turrets(nnh::World& world, const nnh::Hierarchy& tree);

    nnh::ParticleEmitter dust_;
    nnh::ParticleEmitter puff_;
    nnh::ParticleEmitter flash_;
    bool was_on_ground_ = true;
    bool was_running_ = false;
    int hits_ = 0;
};
