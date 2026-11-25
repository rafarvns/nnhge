// Scene binding (spec 028 FR-21, D30 unchanged): the block carries a scene attachment; the scene stack notifies it
// right before the scene's update() (one update() from the UI's input source, so the scene reads this frame's results,
// and a pause scene pushed on top freezes the UI) and right after its on_exit() (reset_input(): presses, repeats,
// touch and pressed looks forgotten; focus, focus memory, modals and values kept).

#include <cstddef>
#include <type_traits>

#include "ui/ui_block.hpp"

namespace nnh {
namespace detail {
namespace {

static_assert(std::is_standard_layout_v<UiBlock>, "the attachment's block is found with offsetof");

UiBlock& block_of(SceneAttachment& node) {
    return *reinterpret_cast<UiBlock*>(reinterpret_cast<unsigned char*>(&node) - offsetof(UiBlock, attachment));
}

void on_scene_hook(SceneAttachment& node, SceneHook hook) {
    UiBlock& b = block_of(node);
    if (b.busy > 0) return; // cannot happen through the scene stack (changes are deferred); stay safe
    if (hook == SceneHook::BeforeUpdate) {
        UiInput in;
        ui_read_input(b, in);
        ui_step(b, in);
        if (b.orphaned) ui_free(b); // destroyed from its own callback (Release)
    } else {
        ui_reset_input(b);
    }
}

} // namespace

void ui_unbind(UiBlock& b) {
    if (b.scene == nullptr) return;
    scene_detach(*b.scene, b.attachment);
    b.scene = nullptr;
}

} // namespace detail

void Ui::bind(Scene& scene) {
    if (block_ == nullptr || block_->scene != nullptr) {
        if (block_ != nullptr) NNH_UI_MISUSE(*block_, "Ui::bind: already bound (ignored)");
        return;
    }
    block_->attachment = detail::SceneAttachment{};
    block_->attachment.notify = &detail::on_scene_hook;
    block_->scene = &scene;
    detail::scene_attach(scene, block_->attachment);
}

void Ui::unbind() {
    if (block_ != nullptr) detail::ui_unbind(*block_);
}

} // namespace nnh
