#pragma once
// The title: press A and it fades into the first room. The loading scene (FR-14): it replaces a room, loads in 40
// slices showing a tip and a progress bar, then starts a second transition to the other room.

#include <nnh/scene.hpp>

struct Demos;

class TitleScene final : public nnh::Scene {
public:
    explicit TitleScene(Demos& demos) : demos_(demos) {}
    void on_enter() override;
    void update() override;
    void draw() override;

private:
    Demos& demos_;
};

class LoadingScene final : public nnh::Scene {
public:
    static constexpr int kSlices = 40;
    explicit LoadingScene(Demos& demos) : demos_(demos) {}
    void on_enter() override;
    void update() override;
    void draw() override;

private:
    Demos& demos_;
    int slices_ = 0;
    bool leaving_ = false;
};
