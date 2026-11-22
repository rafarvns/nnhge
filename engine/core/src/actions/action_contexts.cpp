// Contexts (spec 017 FR-4, FR-5): enabled by hand, or bound to a scene (enabled exactly while it is the top
// scene). A bound context carries a D30 attachment that ends the binding after the scene's on_exit(), so a
// binding lasts one stay on the stack and a scene is never left with a node of a set in its list.

#include "actions/actions_internal.hpp"
#include "nnh/assert.hpp"
#include "nnh/scene.hpp"

#include <cstddef>
#include <type_traits>

namespace nnh {
namespace detail {
namespace {

static_assert(std::is_standard_layout_v<ContextRec>, "the attachment's context is found with offsetof");

ContextRec& context_of(SceneAttachment& node) {
    return *reinterpret_cast<ContextRec*>(reinterpret_cast<unsigned char*>(&node) - offsetof(ContextRec, node));
}

void on_scene_hook(SceneAttachment& node, SceneHook hook) {
    if (hook != SceneHook::AfterExit) return;
    ContextRec& ctx = context_of(node);
    context_unbind(*ctx.block, static_cast<int>(&ctx - ctx.block->ctx));
}

ContextRec* find_context(ActionBlock* b, ContextId id) {
    if (b == nullptr) return nullptr;
    if (id.index < b->contexts && (b->ctx[id.index].flags & context_flag::kDeclared) != 0) return &b->ctx[id.index];
    NNH_LOG_ERROR(actions, "ActionSet: context id %u is not declared (ignored)", static_cast<unsigned>(id.index));
    NNH_ASSERT(false, "nnh::ActionSet: context id %u is not declared", static_cast<unsigned>(id.index));
    return nullptr;
}

} // namespace

bool context_on(const ActionBlock& b, int context) {
    const ContextRec& ctx = b.ctx[context];
    if (ctx.scene != nullptr) return scenes::top() == ctx.scene;
    return (ctx.flags & context_flag::kEnabled) != 0;
}

void context_unbind(ActionBlock& b, int context) {
    ContextRec& ctx = b.ctx[context];
    if (ctx.scene == nullptr) return;
    scene_detach(*ctx.scene, ctx.node);
    ctx.scene = nullptr;
    ctx.flags = static_cast<std::uint8_t>(ctx.flags & ~context_flag::kEnabled);
}

} // namespace detail

void ActionSet::enable(ContextId id, bool on) {
    detail::ContextRec* ctx = detail::find_context(block_, id);
    if (ctx == nullptr) return;
    if (ctx->scene != nullptr) {
        NNH_LOG_ERROR(actions, "ActionSet: enable() on context \"%s\", bound to a scene (ignored)", ctx->name);
        NNH_ASSERT(false, "nnh::ActionSet: enable() on a context bound to a scene: unbind it first");
        return;
    }
    ctx->flags = static_cast<std::uint8_t>(on ? ctx->flags | detail::context_flag::kEnabled
                                              : ctx->flags & ~detail::context_flag::kEnabled);
}

bool ActionSet::enabled(ContextId id) const {
    if (block_ == nullptr || id.index >= block_->contexts) return false;
    return (block_->ctx[id.index].flags & detail::context_flag::kDeclared) != 0 && detail::context_on(*block_, id.index);
}

void ActionSet::bind_context(ContextId id, Scene& scene) {
    detail::ContextRec* ctx = detail::find_context(block_, id);
    if (ctx == nullptr) return;
    if (!scene.on_stack()) {
        NNH_LOG_ERROR(actions, "ActionSet: context \"%s\" bound to a scene that is not on the stack (ignored)",
                      ctx->name);
        NNH_ASSERT(false, "nnh::ActionSet: bind_context() needs a scene on the stack (call it in on_enter())");
        return;
    }
    detail::context_unbind(*block_, id.index);
    ctx->scene = &scene;
    ctx->node.notify = &detail::on_scene_hook;
    detail::scene_attach(scene, ctx->node);
}

void ActionSet::unbind_context(ContextId id) {
    if (detail::find_context(block_, id) != nullptr) detail::context_unbind(*block_, id.index);
}

} // namespace nnh
