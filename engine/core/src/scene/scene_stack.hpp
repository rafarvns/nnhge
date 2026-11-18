#pragma once
// Scene stack with deferred changes. Pure logic (no engine state); unit tested in
// tests/scene/test_scene_stack.cpp. The engine's instance lives in scene/scenes.cpp.

#include "nnh/scene.hpp"

namespace nnh::detail {

class SceneStack {
public:
    /// Queue a change (applied by apply_pending()). Too many pending changes is a bug (NNH_VERIFY).
    void request_push(Scene& scene);
    void request_pop();
    void request_replace(Scene& scene);

    /// Applies queued changes in order, calling on_exit()/on_enter(). Changes requested by those callbacks
    /// are applied too, in the same call.
    void apply_pending();

    /// Top update(), then draw() bottom to top.
    void update_and_draw();

    /// Drops pending changes and exits every scene, top first.
    void clear();

    Scene* top() const { return size_ > 0 ? stack_[size_ - 1] : nullptr; }
    int size() const { return size_; }
    int pending() const { return pending_count_; }

private:
    enum class Op : unsigned char { Push, Pop, Replace };
    struct Request {
        Op op;
        Scene* scene;
    };

    Scene* stack_[scenes::kMaxDepth] = {};
    int size_ = 0;
    Request pending_[scenes::kMaxPending] = {};
    int pending_count_ = 0;

    void enqueue(Op op, Scene* scene);
    void do_push(Scene& scene);
    void do_pop();
    void drop_pending();
};

} // namespace nnh::detail
