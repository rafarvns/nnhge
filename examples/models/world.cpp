// The models example's world (world.hpp): two converted models, three primitives and two upright planes.

#include "world.hpp"

#include <cstdlib>
#include <cstring>
#include <utility>

bool demo_is(const char* name) {
    const char* demo = std::getenv("MODELS_DEMO");
    return demo != nullptr && std::strcmp(demo, name) == 0;
}

namespace {

template <typename T>
bool take(nnh::Result<T>&& r, T& out, const char* what) {
    if (!r) {
        NNH_LOG_ERROR(example, "models: %s: %s", what, r.error().message);
        return false;
    }
    out = std::move(r.value());
    return true;
}

} // namespace

bool World::load() {
    if (!take(nnh::load_texture("art/checker"), checker, "checker") ||
        !take(nnh::load_texture("art/fence"), fence_art, "fence"))
        return false;
    const nnh::Material3D floor_m{.base_color = {255, 255, 255, 255}, .texture = nnh::texture_id(checker)};
    const nnh::Material3D fence_m{.texture = nnh::texture_id(fence_art), .alpha = nnh::AlphaMode::Cutout,
                                  .cull_back = false};
    const nnh::Material3D glass_m{.base_color = {120, 200, 255, 110}, .specular = {255, 255, 255, 255},
                                  .shininess = 5, .alpha = nnh::AlphaMode::Blend, .cull_back = false};
    const nnh::Material3D cube_m{.base_color = {235, 150, 40, 255}, .specular = {200, 200, 200, 255}, .shininess = 4};
    const nnh::Material3D ball_m{.base_color = {70, 110, 230, 255}, .specular = {255, 255, 255, 255}, .shininess = 6};
    return take(nnh::load_mesh("models/ship"), ship, "ship") && take(nnh::load_mesh("models/pylon"), pylon, "pylon") &&
           take(nnh::make_cube(1, cube_m), cube, "cube") && take(nnh::make_sphere(0.6f, 12, 24, ball_m), sphere, "sphere") &&
           take(nnh::make_plane(12, 12, 8, floor_m), floor, "floor") && take(nnh::make_plane(4, 1, 1, fence_m), fence, "fence") &&
           take(nnh::make_plane(2, 2, 1, glass_m), glass, "glass");
}
