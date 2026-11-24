#pragma once
// The models showcase (spec 026 AC-6): a spinning textured ship converted from glTF on a checker floor, a code-built
// cube and sphere, a vertex-coloured pylon, a cut-out fence, a blended glass pane, a directional light and a point
// light circling the scene; a 2D sky below the world layer, the HUD and a name tag (to_screen) above it; touch buttons
// on the bottom screen. Circle Pad: orbit; L/R: zoom; X: perspective/orthographic; SELECT: check; Y: bench; START: quit.

#include <cstdint>
#include <nnh/scene.hpp>

#include "world.hpp"

class ShowcaseScene : public nnh::Scene {
public:
    ShowcaseScene(World& world, Demos& demos) : world_(world), demos_(demos) {}
    void on_enter() override;
    void update() override;
    void draw() override;

    /// The camera for frame `frame` (pure: the screenshots and the script depend on it).
    nnh::Camera3D camera() const;

private:
    void touch_buttons();
    void log_tag();
    void draw_world();
    void draw_sky_and_hud();
    void draw_bottom();

    World& world_;
    Demos& demos_;
    std::uint32_t frame_ = 0;
    float yaw_ = 0.6f;
    float pitch_ = 0.35f;
    float distance_ = 7.0f;
    bool ortho_ = false;
    bool textures_ = true, lighting_ = true, point_light_ = true, culling_ = true, debug_ = false;
    int flash_ = 0;
    bool moved_ = false;
};
