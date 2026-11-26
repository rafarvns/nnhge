#pragma once
// The example's tunables and watches (spec 030 AC-1): the Tuning values registered by pointer, the world's state
// watched, an action. Owned by the scene that registers them (its handles go with it, FR-6).

#include <nnh/live.hpp>

#include "sim.hpp"

struct LiveHandles {
    nnh::live::Handle jump, gravity, run, hitboxes, count, shape, sky, ground, dust, respawn;
    nnh::live::Handle pos, vel, state, frame;
};

/// Registers `t` and `w`'s values. `respawn` runs at the application point when the action is pressed.
void register_live(LiveHandles& h, Tuning& t, World& w, const char* const* state_text, std::uint32_t* frame_counter,
                   void (*respawn)(void*), void* context);

/// Times fx/dust's callback ran (it "rebuilds" the dust burst).
int dust_rebuilds();
