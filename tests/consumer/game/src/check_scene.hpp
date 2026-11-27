#pragma once
// The consumer game's one scene: runs the self-check on entering (and again on SELECT), logs its line, shows PASS/FAIL.
// START quits.

#include <nnh/scene.hpp>

#include "self_check.hpp"

class CheckScene : public nnh::Scene {
public:
    void on_enter() override;
    void update() override;
    void draw() override;

private:
    void check();

    ConsumerCheck result_;
    char line_[96] = {};
};
