#pragma once
// The models example's world (spec 026 AC-6): the meshes and textures every scene draws, loaded once.

#include <nnh/gfx3d.hpp>
#include <nnh/log.hpp>
#include <nnh/mesh.hpp>
#include <nnh/scene.hpp>
#include <nnh/texture.hpp>

NNH_LOG_CHANNEL_EXTERN(example);

struct World {
    nnh::Mesh ship;    ///< assets/models/ship.gltf (textured hull, red wings), converted at build
    nnh::Mesh pylon;   ///< assets/models/pylon.gltf (vertex colours)
    nnh::Mesh cube;    ///< make_cube
    nnh::Mesh sphere;  ///< make_sphere
    nnh::Mesh floor;   ///< make_plane, checker texture
    nnh::Mesh fence;   ///< make_plane turned upright, cut-out texture
    nnh::Mesh glass;   ///< make_plane turned upright, blended
    nnh::Texture checker, fence_art;

    /// Loads everything; logs and returns false on a failure.
    bool load();
};

/// Which scene starts (MODELS_DEMO = showcase | check | bench | smoke).
bool demo_is(const char* name);

/// The scenes, for switching between them.
struct Demos {
    nnh::Scene* showcase = nullptr;
    nnh::Scene* check = nullptr;
    nnh::Scene* bench = nullptr;
    nnh::Scene* smoke = nullptr; ///< the console smoke test of the 3D backend (spec 026 T7.6)
};
