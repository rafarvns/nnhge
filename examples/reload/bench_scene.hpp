#pragma once
// The hot reload example's bench (spec 031 AC-8, RELOAD_DEMO=bench): the showcase's frame (update + draw) timed over
// 300 frames, then 1000 assets::exists + size reads; logs "reload bench: idle avg <ms> worst <ms> over 300 frames" and
// "reload bench: reads <us> per read". Run it before any batch and after a few: the idle averages must agree within
// noise. Pushed by the showcase's X, it pops itself after one round.

#include <nnh/scene.hpp>
#include <nnh/time.hpp>

#include "reload_scene.hpp"

class BenchScene : public nnh::Scene {
public:
    explicit BenchScene(World& world, bool pushed = false) : showcase_(world), pushed_(pushed) {}
    void on_enter() override;
    void update() override;
    void draw() override;

private:
    ReloadScene showcase_;
    bool pushed_ = false;
    int frames_ = 0;
    double total_ms_ = 0, worst_ms_ = 0;
    nnh::Duration start_{};
};
