#pragma once
/// @file nnh/scene.hpp
/// Scenes (title, level, pause menu...) and the scene stack. See docs/api/scene.md and
/// docs/architecture/scenes.md.
///
/// @code
/// class Level final : public nnh::Scene {
///     void on_enter() override { coin_sub_ = nnh::events::subscribe<&Level::on_coin>(this); }
///     void on_exit() override { coin_sub_.reset(); }
///     void update() override { if (nnh::input::pressed(nnh::Button::Start)) nnh::scenes::push(pause_); }
///     void draw() override { /* draw only */ }
///     ...
/// };
///
/// Title title;  Level level;                // the game creates and owns its scenes
/// nnh::run(title);                          // or a manual loop calling nnh::scenes::update_and_draw()
/// @endcode

#include <cstdint>

#include "nnh/assert.hpp"

namespace nnh {

namespace detail {
class SceneStack;
struct SceneAttachment;
struct SceneAccess;
} // namespace detail

/// One game mode. Override what you need; every function is optional.
/// Every virtual function is defined inline on purpose: games may build with RTTI while the engine
/// doesn't, so the vtable and type info must be emitted by the game's own code.
/// The game owns scene objects: a scene must outlive its time on the stack (destroying a scene that is
/// on the stack, or waiting to be pushed, stops the game with an error).
class Scene {
public:
    Scene() = default;
    virtual ~Scene() {
        // One check (one message) keeps every game scene's destructor small (D30's cost budget).
        NNH_VERIFY(!on_stack_ && pending_ == 0 && attachments_ == nullptr,
                   "scene destroyed while still on the scene stack (or waiting to be pushed), or with a TweenSet "
                   "still attached: keep scene objects alive until they are popped, bound sets inside them");
    }
    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    /// Called when the scene enters the stack (push, replace): acquire resources, subscribe to events.
    virtual void on_enter() {}
    /// Called when the scene leaves the stack (pop, replace, end of run(), shutdown()).
    virtual void on_exit() {}
    /// Game logic for one frame. Only the top scene updates.
    virtual void update() {}
    /// Drawing only. Every scene on the stack draws, bottom to top (a pause menu draws over its level).
    virtual void draw() {}

    /// True between on_enter() and on_exit().
    bool on_stack() const { return on_stack_; }

private:
    friend class detail::SceneStack;
    friend struct detail::SceneAccess;
    detail::SceneAttachment* attachments_ = nullptr; // objects bound to this scene (D30), e.g. a TweenSet
    bool on_stack_ = false;
    std::uint8_t pending_ = 0; // queued push/replace requests that point here
};

namespace scenes {

/// Most scenes on the stack at once, and most changes requested in one frame.
inline constexpr int kMaxDepth = 8;
inline constexpr int kMaxPending = 8;

/// Requests a change. Changes take effect at the start of the next nnh::frame() (the end of this frame),
/// in request order, so a scene never leaves the stack in the middle of its own update().
void push(Scene& scene);    ///< `scene` goes on top; the one below stops updating but still draws
void pop();                 ///< removes the top scene; the one below updates again
void replace(Scene& scene); ///< pop + push in one step (title -> level)

/// Updates the top scene, then draws every scene bottom to top. nnh::run() calls it each frame; call it
/// yourself in a manual `while (nnh::frame())` loop.
void update_and_draw();

/// The scene on top, or nullptr.
Scene* top();
/// Scenes on the stack (pending changes not counted).
int size();

} // namespace scenes

/// Runs the game with scenes: pushes `first`, then each frame calls scenes::update_and_draw(), until
/// the stack becomes empty or the system asks to quit (nnh::frame() returns false). On the way out, every
/// scene left on the stack gets on_exit(). Call nnh::init() before and nnh::shutdown() after.
void run(Scene& first);

} // namespace nnh
