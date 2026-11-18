#pragma once
// Pushed over the level: the level stops updating but keeps drawing underneath.

#include <nnh/scene.hpp>

class PauseScene final : public nnh::Scene {
public:
    void update() override;
    void draw() override;
};
