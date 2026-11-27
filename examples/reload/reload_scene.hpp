#pragma once
// The hot reload showcase (spec 031): the backdrop, the hero, the room with its platform, the two enemies moving at
// their data speeds, the spinning gem, the walking sprite, a translated greeting, a line in the HUD font, the music and a
// corner line with reload_count() and the last batch's names. A plays the blip, X runs the bench. At frame 60 and on the
// frame after each AssetsReloaded it logs the self-check line "reload: sum XXXXXXXX after N" (tests/check.nnhs compares
// them with the PC's). START quits.

#include <nnh/assets.hpp>
#include <nnh/events.hpp>
#include <nnh/scene.hpp>

#include "world.hpp"

class ReloadScene : public nnh::Scene {
public:
    explicit ReloadScene(World& world) : world_(world) {}
    /// X pushes `bench` (tests/timing.nnhs).
    void set_bench(nnh::Scene* bench) { bench_ = bench; }
    void on_enter() override;
    void update() override;
    void draw() override;

private:
    void report();

    World& world_;
    nnh::Scene* bench_ = nullptr;
    nnh::Subscription reloaded_;
    int report_at_ = 60; ///< the frame that logs the self-check line
    int frame_ = 0;
    float goblin_x_ = 40, bat_x_ = 200;
};
