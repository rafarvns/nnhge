#pragma once
// The console smoke test of the 3D backend (spec 026 T7.6; MODELS_DEMO=smoke): a flat (unlit) cube; A switches to a
// lit, textured one and logs "smoke: lit". START quits. The backend's own Development self-test logs "3d tilt: ..." at
// init.

#include <cstdint>
#include <nnh/scene.hpp>

#include "world.hpp"

class SmokeScene : public nnh::Scene {
public:
    explicit SmokeScene(World& world) : world_(world) {}
    void on_enter() override;
    void update() override;
    void draw() override;

private:
    World& world_;
    bool lit_ = false;
    std::uint32_t frame_ = 0;
};
