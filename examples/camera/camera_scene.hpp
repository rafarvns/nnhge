#pragma once
// The camera playground: a CameraRig follows the square the player moves through the rooms, with its dead zone and
// limits drawn. Controls (every 3DS has them):
//   Circle Pad / D-Pad  move          A  trauma 0.4 (shake)       X  zoom 1 → 1.5 → 2 → 3 → 0.5
//   Y  smoothing on/off               L  gap 0 / 24 (tall world)  R  mode: single view → tall world → two views
//   touch the bottom screen (tall world, two views): a marker at the world point under the finger
//   SELECT  self-check                START  quit

#include "demos.hpp"
#include "world.hpp"

#include <nnh/camera.hpp>
#include <nnh/scene.hpp>

class CameraScene final : public nnh::Scene {
public:
    enum class Mode { Single, Tall, Two };

    CameraScene(const Demos& demos, world::World& world);

    /// CAMERA_DEMO: zoom1, zoom2, zoom05, tall, two (and nothing: zoom 1, single view).
    void preset();

    void on_enter() override;
    void update() override;
    void draw() override;

    /// What the HUD shows (hud.cpp).
    Mode mode() const { return mode_; }
    float zoom() const;
    bool smoothing() const { return smoothing_; }
    const nnh::CameraRig& rig() const { return rig_; }

private:
    void configure(); ///< settings of both rigs from the current mode, zoom, smoothing and gap
    void controls();
    void touch();
    void draw_screen(nnh::Screen screen) const;

    const Demos& demos_;
    world::World& world_;
    world::Square player_;
    nnh::CameraRig rig_;    ///< follows the player (top screen, or both in tall world)
    nnh::CameraRig second_; ///< two views: the bottom screen follows the automatic target
    Mode mode_ = Mode::Single;
    int zoom_index_ = 0;
    bool smoothing_ = true;
    float gap_ = 0;
    int frame_ = 0;
    bool has_marker_ = false;
    nnh::Vec2 marker_{};
};
