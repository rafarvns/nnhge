#pragma once
// A room of the showcase: its tile map on the top screen, the HUD on the bottom one, a square moved with the Circle
// Pad, and the controls that start room changes with the chosen transition (spec 023 AC-5).

#include <nnh/math.hpp>
#include <nnh/scene.hpp>
#include <nnh/transition.hpp>

struct Demos;

class RoomScene final : public nnh::Scene {
public:
    enum class Change : std::uint8_t { Plain, LoadingStep, LoadingScene };

    RoomScene(Demos& demos, int id) : demos_(demos), id_(id) {}
    void on_enter() override;
    void update() override;
    void draw() override;

    /// Starts a room change with the current settings (false when another transition runs).
    bool change(Change kind, int out_frames = 24);
    /// The music and log of a room change: the loading scene's second transition uses them too.
    static void covered(Demos& demos, bool play_music);
    static void finished(Demos& demos);

private:
    void controls();
    void shot();
    nnh::transition::LoadStep slice();

    Demos& demos_;
    int id_;
    nnh::Vec2 square_{192, 112};
    int frames_ = 0;
    int slices_ = 0;
};
