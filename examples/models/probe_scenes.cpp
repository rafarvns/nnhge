// The check's probe scenes (probe_scenes.hpp). Expected colours are FR-10 worked out by hand for each fixed picture.

#include "probe_scenes.hpp"

#include <nnh/gfx.hpp>
#include <nnh/transform.hpp>
#include <utility>

namespace {

using nnh::Screen;

nnh::Transform3D upright(nnh::Vec3 at, nnh::Vec3 scale = {1, 1, 1}) { // a plane facing +y turned to face +z
    return {.position = at, .rotation = nnh::quat_axis_angle({1, 0, 0}, nnh::kQuarterTurn), .scale = scale};
}

nnh::Material3D unlit(nnh::Color c, nnh::AlphaMode alpha = nnh::AlphaMode::Opaque) {
    return {.base_color = c, .alpha = alpha, .lit = false, .cull_back = false};
}

template <typename T>
bool take(nnh::Result<T>&& r, T& out) {
    if (!r) return false;
    out = std::move(r.value());
    return true;
}

} // namespace

bool ProbeScenes::load(const World& world) {
    world_ = &world;
    const nnh::Material3D tex{.texture = nnh::texture_id(world.checker), .lit = false};
    const nnh::Material3D grey{.base_color = {200, 200, 200, 255}};
    return take(nnh::make_plane(2, 2, 1, tex), quad_tex_) && take(nnh::make_plane(1, 1, 1), quad_) &&
           take(nnh::make_sphere(1, 24, 48, grey), sphere_) && take(nnh::make_cube(2), cube_);
}

const char* ProbeScenes::name(int i) {
    static const char* const names[kScenes] = {"flat", "textured", "lit", "cutout", "blend", "2d"};
    return i >= 0 && i < kScenes ? names[i] : "?";
}

void ProbeScenes::draw(int i) const {
    nnh::Camera3D cam;
    cam.projection = nnh::Projection::Orthographic;
    cam.ortho_height = 2.4f; // 100 px per world unit
    cam.far_plane = 20;
    nnh::gfx3d::set_camera(Screen::Top, cam);
    nnh::Lighting l;
    l.ambient = {0, 0, 0, 255};
    l.lights[0] = {.kind = nnh::LightKind::Directional, .vector = {1, 0, 0}};
    nnh::gfx3d::set_lighting(Screen::Top, l);
    nnh::gfx3d::set_world_layer(Screen::Top, 8);
    const nnh::Material3D red = unlit({255, 0, 0, 255}), green = unlit({0, 255, 0, 255});
    switch (i) {
    case 0: {
        const nnh::Material3D m = unlit({200, 100, 50, 255});
        nnh::gfx3d::draw(Screen::Top, cube_, nnh::Transform3D{}, {.material = &m});
        break;
    }
    case 1: nnh::gfx3d::draw(Screen::Top, quad_tex_, upright({})); break;
    case 2: nnh::gfx3d::draw(Screen::Top, sphere_, nnh::Transform3D{}); break;
    case 3: {
        const nnh::Material3D fence{.texture = nnh::texture_id(world_->fence_art), .alpha = nnh::AlphaMode::Cutout,
                                    .lit = false, .cull_back = false};
        nnh::gfx3d::draw(Screen::Top, quad_, upright({0, 0, -0.5f}, {4, 1, 2.4f}), {.material = &red});
        nnh::gfx3d::draw(Screen::Top, world_->fence, upright({}), {.material = &fence});
        break;
    }
    case 4: {
        const nnh::Material3D glass = unlit({255, 255, 255, 128}, nnh::AlphaMode::Blend);
        const nnh::Material3D behind = unlit({255, 0, 0, 128}, nnh::AlphaMode::Blend);
        nnh::gfx3d::draw(Screen::Top, quad_, upright({0, 0, 0}, {4, 1, 2.4f}), {.material = &green});
        nnh::gfx3d::draw(Screen::Top, quad_, upright({-1, 0, 0.5f}, {2, 1, 2.4f}), {.material = &glass});
        nnh::gfx3d::draw(Screen::Top, quad_, upright({1, 0, -0.5f}, {2, 1, 2.4f}), {.material = &behind});
        break;
    }
    default:
        nnh::gfx::fill_rect(Screen::Top, {0, 0, 400, 240}, {0, 0, 255, 255}, {.layer = 0, .ignore_camera = true});
        nnh::gfx3d::draw(Screen::Top, cube_, nnh::Transform3D{}, {.material = &red});
        nnh::gfx::fill_rect(Screen::Top, {190, 110, 20, 20}, {255, 255, 255, 255}, {.layer = 9, .ignore_camera = true});
        break;
    }
}

int ProbeScenes::expect(int i, ProbeExpect* out) const {
    switch (i) {
    case 0:
        out[0] = {{200, 120}, {200, 100, 50, 255}};
        out[1] = {{50, 120}, {20, 24, 40, 255}}; // the clear colour
        return 2;
    case 1: // u = (x − 100)/200, v = (y − 20)/200; 8 checker squares across
        out[0] = {{130, 50}, {230, 230, 230, 255}};
        out[1] = {{155, 50}, {70, 90, 70, 255}};
        return 2;
    case 2: // N·L = x on the unit sphere (light from +x, no ambient): 200 × x
        out[0] = {{250, 120}, {101, 101, 101, 255}, 10};
        out[1] = {{290, 120}, {181, 181, 181, 255}, 10};
        out[2] = {{150, 120}, {0, 0, 0, 255}, 10};
        return 3;
    case 3: // the fence: 64 × 32 texels over 400 × 100 px, posts at x % 16 < 3, rails at y % 16 in 4..6
        out[0] = {{50, 101}, {255, 0, 0, 255}};   // a hole: the red quad behind
        out[1] = {{7, 101}, {150, 100, 50, 255}}; // a post
        return 2;
    case 4: // white at 128 over green: (255·128 + c·127 + 127) / 255
        out[0] = {{100, 120}, {128, 255, 128, 255}, 3};
        out[1] = {{300, 120}, {0, 255, 0, 255}, 3}; // the red pane behind the green fails the depth test
        return 2;
    default:
        out[0] = {{200, 120}, {255, 255, 255, 255}}; // HUD (layer 9) over the world
        out[1] = {{50, 120}, {0, 0, 255, 255}};      // sky (layer 0) behind it
        out[2] = {{150, 120}, {255, 0, 0, 255}};     // the cube
        return 3;
    }
}
