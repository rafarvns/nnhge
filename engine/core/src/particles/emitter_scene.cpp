// Scene-bound emitters (spec 024 FR-17, decision D30 unchanged): the block carries a scene attachment; the scene stack
// notifies it right before the scene's update() (one update()) and right after its on_exit() (a clear). A pause scene
// pushed on top freezes the effect: only the top scene is updated.

#include <cstddef>
#include <type_traits>

#include "particles/particle_block.hpp"

namespace nnh {
namespace detail {
namespace {

static_assert(std::is_standard_layout_v<ParticleBlock>, "the attachment's block is found with offsetof");

ParticleBlock& block_of(SceneAttachment& node) {
    return *reinterpret_cast<ParticleBlock*>(reinterpret_cast<unsigned char*>(&node) -
                                             offsetof(ParticleBlock, attachment));
}

void on_scene_hook(SceneAttachment& node, SceneHook hook) {
    ParticleBlock& b = block_of(node);
    if (hook == SceneHook::BeforeUpdate) {
        emitter_step(b);
    } else {
        emitter_clear(b);
    }
}

} // namespace

void particle_unbind(ParticleBlock& b) {
    if (b.scene == nullptr) return;
    scene_detach(*b.scene, b.attachment);
    b.scene = nullptr;
}

} // namespace detail

void ParticleEmitter::bind(Scene& scene) {
    if (block_ == nullptr || block_->scene != nullptr) {
        NNH_PARTICLE_MISUSE("ParticleEmitter::bind: an empty emitter, or already bound (ignored)");
        return;
    }
    block_->attachment = detail::SceneAttachment{};
    block_->attachment.notify = &detail::on_scene_hook;
    block_->scene = &scene;
    detail::scene_attach(scene, block_->attachment);
}

void ParticleEmitter::unbind() {
    if (block_ == nullptr) {
        NNH_PARTICLE_MISUSE("ParticleEmitter::unbind on an empty emitter");
        return;
    }
    detail::particle_unbind(*block_);
}

} // namespace nnh
