#pragma once
// The example's scenes and shared state (TRANSITIONS_DEMO picks the first scene: showcase (default), check, bench;
// TRANSITIONS_SHOT=<fade|wipe|split|blinds|iris|diamond|curtain|tall|loading> starts a room change at frame 20 for
// screenshots).

#include <nnh/log.hpp>
#include <nnh/scene.hpp>

#include "curtain.hpp"
#include "input_log.hpp"
#include "options_menu.hpp"
#include "world.hpp"

NNH_LOG_CHANNEL_EXTERN(example);

class TransitionsCheck;

/// TRANSITIONS_DEMO is `name`.
bool demo_is(const char* name);
/// TRANSITIONS_SHOT, or nullptr.
const char* shot_mode();

struct Demos {
    World* world = nullptr;
    Curtain* curtain = nullptr;
    InputLog* input = nullptr;
    nnh::Scene* title = nullptr;
    nnh::Scene* rooms[2] = {};
    nnh::Scene* loading = nullptr;
    nnh::Scene* bench = nullptr;
    TransitionsCheck* check = nullptr;
    Settings settings{};
    int loading_target = 0; ///< the room the loading scene goes to
};
