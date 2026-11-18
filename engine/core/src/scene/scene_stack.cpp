// Scene stack with deferred changes (see scene_stack.hpp).

#include "scene/scene_stack.hpp"

#include "nnh/assert.hpp"

namespace nnh::detail {

void SceneStack::enqueue(Op op, Scene* scene) {
    NNH_VERIFY(pending_count_ < scenes::kMaxPending, "too many scene changes in one frame (max %d)",
               scenes::kMaxPending);
    if (scene != nullptr) ++scene->pending_;
    pending_[pending_count_++] = Request{op, scene};
}

void SceneStack::request_push(Scene& scene) { enqueue(Op::Push, &scene); }
void SceneStack::request_pop() { enqueue(Op::Pop, nullptr); }
void SceneStack::request_replace(Scene& scene) { enqueue(Op::Replace, &scene); }

void SceneStack::do_push(Scene& scene) {
    NNH_VERIFY(!scene.on_stack_, "scene pushed while already on the scene stack");
    NNH_VERIFY(size_ < scenes::kMaxDepth, "scene stack full (max %d scenes)", scenes::kMaxDepth);
    stack_[size_++] = &scene;
    scene.on_stack_ = true;
    scene.on_enter();
}

void SceneStack::do_pop() {
    NNH_VERIFY(size_ > 0, "scenes::pop() with an empty scene stack");
    Scene* scene = stack_[--size_];
    stack_[size_] = nullptr;
    scene->on_stack_ = false;
    scene->on_exit();
}

void SceneStack::apply_pending() {
    // on_enter()/on_exit() may request more changes: they go to the (emptied) buffer and are applied in the
    // next round. A few rounds are normal (a scene that immediately pushes another); many mean a loop.
    for (int round = 0; pending_count_ > 0; ++round) {
        NNH_VERIFY(round < scenes::kMaxPending, "scene changes keep requesting more changes (a loop?)");
        Request batch[scenes::kMaxPending];
        const int count = pending_count_;
        for (int i = 0; i < count; ++i) batch[i] = pending_[i];
        pending_count_ = 0;
        for (int i = 0; i < count; ++i) {
            const Request request = batch[i];
            if (request.scene != nullptr) --request.scene->pending_;
            switch (request.op) {
            case Op::Push: do_push(*request.scene); break;
            case Op::Pop: do_pop(); break;
            case Op::Replace: // on an empty stack, the same as push
                if (size_ > 0) do_pop();
                do_push(*request.scene);
                break;
            }
        }
    }
}

void SceneStack::update_and_draw() {
    if (size_ == 0) return;
    stack_[size_ - 1]->update(); // requests from update() are only queued: the stack is stable while drawing
    for (int i = 0; i < size_; ++i) stack_[i]->draw();
}

void SceneStack::drop_pending() {
    for (int i = 0; i < pending_count_; ++i) {
        if (pending_[i].scene != nullptr) --pending_[i].scene->pending_;
    }
    pending_count_ = 0;
}

void SceneStack::clear() {
    drop_pending();
    while (size_ > 0) do_pop();
    drop_pending(); // changes requested by on_exit() don't apply anymore
}

} // namespace nnh::detail
