#pragma once
// The self-check screen: runs check.hpp's run_data_check() on entering, logs the verdict and shows it as a large
// PASS/FAIL banner on both screens. B or START goes back.

#include <nnh/scene.hpp>

#include "check.hpp"

class CheckScene : public nnh::Scene {
public:
    void on_enter() override;
    void update() override;
    void draw() override;

private:
    DataCheckResult result_;
    char line_[128] = {};
};
