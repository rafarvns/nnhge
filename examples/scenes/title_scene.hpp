#pragma once
// First scene: A starts the level, START quits (pops the last scene, so nnh::run() returns).

#include <nnh/scene.hpp>

class TitleScene final : public nnh::Scene {
public:
    void set_level(nnh::Scene& level) { level_ = &level; }

    void on_enter() override;
    void update() override;
    void draw() override;

private:
    nnh::Scene* level_ = nullptr;
};
