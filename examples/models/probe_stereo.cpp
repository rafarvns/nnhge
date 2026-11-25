// The check's stereo probe scene (probe_scenes.hpp, spec 026 AC-7/FR-14). Default camera (0, 0, 5) looking down −z,
// 60° view, focal distance 5: the focal plane is z = 0. Eye separation 0.25 (strength = 0.25 / slider): an eye at
// ±0.125 shifts a point at distance Z by ∓P00·0.125·(1/Z − 1/5) in NDC (P00 = 1.0392), so the green card at z = 3
// (2 ahead) lands 7.8 px right of the centre in the left eye and 7.8 px left of it in the right eye; it is 0.2 wide,
// 20.8 px at that distance. The red wall on the focal plane is the same in both eyes. Unlit: exact colours.

#include <nnh/transform.hpp>

#include "probe_scenes.hpp"

void ProbeScenes::draw_stereo(float slider) const {
    using nnh::Screen;
    nnh::Camera3D cam;
    cam.stereo.strength = slider > 0.0f ? 0.25f / slider : 0.0f; // the same picture at any slider above 0
    nnh::gfx3d::set_camera(Screen::Top, cam);
    nnh::gfx3d::set_world_layer(Screen::Top, 8);
    const auto facing = nnh::quat_axis_angle({1, 0, 0}, nnh::kQuarterTurn); // the plane (facing +y) towards +z
    const nnh::Material3D red{.base_color = {255, 0, 0, 255}, .lit = false, .cull_back = false};
    const nnh::Material3D green{.base_color = {0, 255, 0, 255}, .lit = false, .cull_back = false};
    nnh::gfx3d::draw(Screen::Top, quad_, nnh::Transform3D{.rotation = facing, .scale = {8, 1, 6}}, {.material = &red});
    nnh::gfx3d::draw(Screen::Top, quad_,
                     nnh::Transform3D{.position = {0, 0, 3}, .rotation = facing, .scale = {0.2f, 1, 0.4f}},
                     {.material = &green});
}

int ProbeScenes::expect_stereo(int eye, ProbeExpect* out) {
    const nnh::Color red{255, 0, 0, 255}, green{0, 255, 0, 255};
    // The card spans 197–218 px in the left eye and 182–203 in the right one (the centre view: 190–210).
    out[0] = {{212, 120}, eye == 0 ? green : red};
    out[1] = {{188, 120}, eye == 0 ? red : green};
    out[2] = {{100, 120}, red}; // the wall: on the focal plane, the same in both eyes
    return 3;
}
