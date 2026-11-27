#pragma once
// What the hot reload example loads (spec 031): handles that follow a reload by themselves (textures, the sound, the
// mesh, the strings) and copies the game reloads when a batch names them (the enemies' data, the room).

#include <nnh/audio.hpp>
#include <nnh/log.hpp>
#include <nnh/mesh.hpp>
#include <nnh/texture.hpp>
#include <nnh/tilemap.hpp>

#include "data_types.hpp"
#include "self_check.hpp"

NNH_LOG_CHANNEL_EXTERN(example);

bool demo_is(const char* name);

struct World {
    nnh::Texture backdrop, hero;
    nnh::Tilemap room;
    Enemy goblin, bat;
    nnh::Sound blip;
    nnh::Mesh gem;
    char last[96] = "";   ///< the last batch's names
    int platform = -1;    ///< the room's platform object

    bool load();
    /// After an AssetsReloaded: loads again what the batch named. @return false if a reload failed (logged).
    bool refresh();
    ReloadSnapshot snapshot() const;
};
