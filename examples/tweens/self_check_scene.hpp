#pragma once
// Plays the self-check (self_check.hpp) one frame per frame: logs the rounding probe, a checksum line every
// 60 frames and the verdict "tweens: PASS <sum>" or "tweens: FAIL ..." (examples/tweens/tests/check.nnhs
// waits for it). SELECT goes to the bench; START quits.

#include "demos.hpp"
#include "self_check.hpp"

#include <nnh/scene.hpp>
#include <optional>

class SelfCheckScene final : public nnh::Scene {
public:
    explicit SelfCheckScene(const Demos& demos) : demos_(demos) {}

    void on_enter() override;
    void on_exit() override { check_.reset(); }
    void update() override;
    void draw() override;

private:
    const Demos& demos_;
    std::optional<SelfCheck> check_; // made on enter: the script starts again each time
    bool rounding_ok_ = false;
    char last_line_[80] = {};
    char verdict_[96] = {};
};
