#pragma once
// The self-check's scenes (spec 023 AC-3): a driver at the bottom of the stack, drawn first every frame (it ticks the
// script), and three plain scenes A, B, C that the script moves between with transitions. Each plain scene draws its
// own colour over both screens (layer 0) and a small square (layer 15) and reports its callbacks to the script.

#include <nnh/gfx.hpp>
#include <nnh/scene.hpp>

class TransitionsCheck;

/// A, B or C.
class CheckScene final : public nnh::Scene {
public:
    CheckScene(TransitionsCheck& check, int id, nnh::Color color) : check_(check), id_(id), color_(color) {}
    void on_enter() override;
    void on_exit() override;
    void update() override;
    void draw() override;

private:
    TransitionsCheck& check_;
    int id_;
    nnh::Color color_;
};

/// The bottom scene: its draw() runs first in every frame (transition or not) and plays one frame of the script.
class CheckDriver final : public nnh::Scene {
public:
    explicit CheckDriver(TransitionsCheck& check) : check_(check) {}
    void on_enter() override;
    void draw() override;

private:
    TransitionsCheck& check_;
};
