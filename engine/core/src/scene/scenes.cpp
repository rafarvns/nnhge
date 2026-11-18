// The engine's scene stack and nnh::run() (nnh/scene.hpp).

#include "nnh/app.hpp"
#include "nnh/scene.hpp"
#include "scene/scene_stack.hpp"
#include "scene/scenes_internal.hpp"

namespace nnh {
namespace {

detail::SceneStack g_stack;

} // namespace

void detail::scenes_apply_pending() { g_stack.apply_pending(); }
void detail::scenes_shutdown() { g_stack.clear(); }

void scenes::push(Scene& scene) { g_stack.request_push(scene); }
void scenes::pop() { g_stack.request_pop(); }
void scenes::replace(Scene& scene) { g_stack.request_replace(scene); }
void scenes::update_and_draw() { g_stack.update_and_draw(); }
Scene* scenes::top() { return g_stack.top(); }
int scenes::size() { return g_stack.size(); }

void run(Scene& first) {
    scenes::push(first);
    while (frame() && g_stack.size() > 0) g_stack.update_and_draw();
    g_stack.clear();
}

} // namespace nnh
