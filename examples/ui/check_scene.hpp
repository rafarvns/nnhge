#pragma once
// The self-check scene (spec 028 AC-4): self_check.hpp's script at 10 script frames per display frame (its UI drawn
// once per display frame), logging "ui: check started", "ui: <frame> sum <checksum>" every 600 frames, then
// "ui: PASS <checksum>" (or "ui: FAIL frame <f> expected <x> got <y>"), then back (SELECT sooner). Y or UI_DEMO=check.

#include <memory>
#include <nnh/scene.hpp>

#include "demos.hpp"
#include "self_check.hpp"

class CheckScene final : public nnh::Scene {
public:
    static constexpr int kPerFrame = 10;

    explicit CheckScene(Demo& demo) : demo_(demo) {}
    void on_enter() override;
    void on_exit() override { check_.reset(); }
    void update() override;
    void draw() override;

private:
    Demo& demo_;
    std::unique_ptr<ui_check::SelfCheck> check_;
    char line_[64] = {};
    char verdict_[96] = {};
    bool failed_ = false;
    int linger_ = 0;
};
