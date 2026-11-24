#pragma once
// The self-check scene (spec 025 AC-5): self_check.hpp's script, 8 searches per frame (64 frames), logging
// "pathfinding: <n> sum <checksum>" every 64 searches and "pathfinding: PASS <checksum>" (or "pathfinding: FAIL search
// <n> expected <x> got <y>" at the first checkpoint that differs), then back to the showcase.

#include <memory>
#include <nnh/scene.hpp>

#include "demos.hpp"
#include "self_check.hpp"

class CheckScene final : public nnh::Scene {
public:
    static constexpr int kPerFrame = 8;

    explicit CheckScene(const Demos& demos) : demos_(demos) {}

    void on_enter() override;
    void on_exit() override { check_.reset(); }
    void update() override;
    void draw() override;

private:
    const Demos& demos_;
    std::unique_ptr<pathfinding_demo::SelfCheck> check_; // ~90 KB of buffers: on the heap
    char line_[96] = {};
    char verdict_[96] = {};
    bool failed_ = false;
    int linger_ = 0;
};
