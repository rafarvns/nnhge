#pragma once
// The self-check scene (spec 024 AC-6): self_check.hpp's script at 10 script frames per display frame, logging
// "particles: <frame> sum <checksum>" every 600 frames, then "particles: PASS <checksum>" (or "particles: FAIL frame <f>
// expected <x> got <y>"), draws the last frame's particles and logs "particles: stats drawn <n> of <max> dropped <d>",
// then goes back to the showcase (SELECT sooner).

#include <memory>
#include <nnh/scene.hpp>

#include "demos.hpp"
#include "self_check.hpp"

class CheckScene final : public nnh::Scene {
public:
    static constexpr int kPerFrame = 10;

    explicit CheckScene(const Demos& demos) : demos_(demos) {}

    void on_enter() override;
    void on_exit() override { check_.reset(); }
    void update() override;
    void draw() override;

private:
    const Demos& demos_;
    std::unique_ptr<particles_check::SelfCheck> check_; // 8 emitters (~150 KB): on the heap
    char line_[96] = {};
    char verdict_[96] = {};
    bool failed_ = false;
    int linger_ = 0;
};
