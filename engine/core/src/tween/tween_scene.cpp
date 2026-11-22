// Scene-bound sets (spec 015 FR-15, decision D30): the block carries a scene attachment; the scene stack
// notifies it right before the scene's update() (one update pass) and right after its on_exit() (a silent
// clear). The dependency points one way: tweens use the scene module's internal header; scenes never name
// tweens.

#include "tween/tween_internal.hpp"

#include <cstddef>
#include <type_traits>

namespace nnh {
namespace detail {
namespace {

static_assert(std::is_standard_layout_v<TweenBlock>, "the attachment's block is found with offsetof");

TweenBlock& block_of(SceneAttachment& node) {
    return *reinterpret_cast<TweenBlock*>(reinterpret_cast<unsigned char*>(&node) -
                                          offsetof(TweenBlock, attachment));
}

void on_scene_hook(SceneAttachment& node, SceneHook hook) {
    TweenBlock& b = block_of(node);
    if (b.busy != 0) return; // cannot happen through the scene stack (changes are deferred); stay safe
    if (hook == SceneHook::BeforeUpdate) {
        tween_pass(b, b.speed_ticks);
    } else {
        tween_kill_all(b); // the scene left: nothing it started may write into it or call back into it
    }
}

} // namespace

void tween_unbind(TweenBlock& b) {
    if (b.scene == nullptr) return;
    scene_detach(*b.scene, b.attachment);
    b.scene = nullptr;
}

} // namespace detail

TweenSet::TweenSet(Scene& scene, const TweenSetConfig& config) : TweenSet(config) {
    if (block_ == nullptr) return;
    block_->scene = &scene;
    block_->attachment.notify = &detail::on_scene_hook;
    detail::scene_attach(scene, block_->attachment);
}

} // namespace nnh
