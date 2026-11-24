#pragma once
// The scene stack's transition hook (decision D38, spec 023): the scene module's API for the transition module,
// which it never names (dependencies point down, as D30 and D32). scenes::update_and_draw() delegates to the hook's
// frame while it is set (one null check per frame otherwise); run() keeps looping while it is set, and the hook is
// cancelled before the stack is cleared (end of run(), shutdown()). Internal; tests: tests/scene/test_scene_hook.cpp.

#include <cstdint>

#include "nnh/scene.hpp"

namespace nnh::detail {

struct SceneTransitionHook {
    void (*frame)();  ///< replaces SceneStack::update_and_draw() for the frame
    void (*cancel)(); ///< silent: no callbacks, the waiting scene released
};

/// Set only while a transition runs (scenes.cpp defines it null).
extern const SceneTransitionHook* g_scene_transition;

/// The stack's two halves, for the hook's frame: the top scene's update (D30's BeforeUpdate first), then every
/// scene's draw, bottom to top.
void scenes_update_top();
void scenes_draw_all();

/// A transition's stack change, through the stack's normal deferred path (applied at the start of the next frame).
enum class SceneChange : std::uint8_t { Push, Replace, TransitionPop };
/// TransitionPop is tolerant: on a stack the game emptied meanwhile it pops nothing (silently; the caller logs).
void scenes_request_change(SceneChange change, Scene* scene);

/// The queued push/replace requests that point at `scene` (Scene::pending_): a scene waiting in a transition holds
/// one, so destroying it or pushing it elsewhere meanwhile is caught as for a pending push.
std::uint8_t& scene_pending_count(Scene& scene);

} // namespace nnh::detail
