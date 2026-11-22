// Declaring contexts and actions (spec 017 FR-1, FR-2, FR-6, FR-8): name rules, ids, duplicates, FNV-1a hash
// collisions, sources that fit the kind. Misuse asserts in Debug/Development and is refused with a log in
// Release.

#include "actions/actions_internal.hpp"
#include "nnh/assert.hpp"
#include "nnh/detail/name_hash.hpp"

#include <cstring>

namespace nnh {
namespace detail {
namespace {

/// 1..max characters of [a-z0-9_].
bool valid_name(const char* name, std::size_t max) {
    if (name == nullptr) return false;
    std::size_t n = 0;
    for (; name[n] != '\0'; ++n) {
        const char c = name[n];
        if (n >= max || !((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false;
    }
    return n > 0;
}

/// A composite declared with a value that is not one button (Source::buttons() maps it to kNoButton).
bool has_empty_part(const PackedSource& p) {
    const int parts = p.kind == Source::Kind::Buttons2 ? 2 : p.kind == Source::Kind::Buttons4 ? 4 : 0;
    for (int i = 0; i < parts; ++i) {
        if (p.d[i] == Source::kNoButton) return true;
    }
    return false;
}

bool refuse(const char* what, const char* name) {
    NNH_LOG_ERROR(actions, "ActionSet: %s (\"%s\"): ignored", what, name != nullptr ? name : "");
    NNH_ASSERT(false, "nnh::ActionSet: %s", what);
    return false;
}

/// Why `desc` can't be declared as action `id`, or nullptr.
const char* problem(const ActionBlock& b, ActionId id, const ActionDesc& desc) {
    if (id.index >= b.capacity) return "action id outside the set's capacity";
    if ((b.actions[id.index].flags & action_flag::kDeclared) != 0) return "action id declared twice";
    if (!valid_name(desc.name, kMaxActionName)) return "action name must be 1-31 characters of [a-z0-9_]";
    if (desc.context.index >= b.contexts || (b.ctx[desc.context.index].flags & context_flag::kDeclared) == 0) {
        return "action in an undeclared context";
    }
    const std::uint32_t hash = fnv1a32(desc.name);
    for (int i = 0; i < b.capacity; ++i) {
        const ActionRec& r = b.actions[i];
        if ((r.flags & action_flag::kDeclared) == 0) continue;
        if (std::strcmp(r.name, desc.name) == 0) return "action name declared twice";
        if (r.hash == hash) return "action name hash collides with another action's (rename one)";
    }
    for (const Binding& binding : desc.bindings) {
        const PackedSource p = pack(binding.source);
        if (!source_fits(desc.kind, p.kind)) return "a binding's source doesn't fit the action kind";
        if (!source_data_valid(p) || has_empty_part(p)) return "a binding's source is invalid";
    }
    return nullptr;
}

void copy_name(char* out, const char* name) {
    std::size_t n = 0;
    for (; name[n] != '\0'; ++n) out[n] = name[n];
    out[n] = '\0';
}

} // namespace
} // namespace detail

bool ActionSet::add_context(ContextId id, const char* name, bool enabled) {
    using namespace detail;
    if (block_ == nullptr) return refuse("add_context() on an empty set", name);
    if (id.index >= block_->contexts) return refuse("context id outside the set's capacity", name);
    if (!valid_name(name, kMaxContextName)) return refuse("context name must be 1-15 characters of [a-z0-9_]", name);
    ContextRec& ctx = block_->ctx[id.index];
    if (id.index != 0 && (ctx.flags & context_flag::kDeclared) != 0) return refuse("context declared twice", name);
    for (int i = 0; i < block_->contexts; ++i) {
        if (i != id.index && (block_->ctx[i].flags & context_flag::kDeclared) != 0 &&
            std::strcmp(block_->ctx[i].name, name) == 0) {
            return refuse("context name declared twice", name);
        }
    }
    copy_name(ctx.name, name);
    ctx.flags = static_cast<std::uint8_t>(context_flag::kDeclared | (enabled ? context_flag::kEnabled : 0));
    return true;
}

bool ActionSet::add(ActionId id, const ActionDesc& desc) {
    using namespace detail;
    if (block_ == nullptr) return refuse("add() on an empty set", desc.name);
    if (const char* why = problem(*block_, id, desc)) return refuse(why, desc.name);
    ActionRec& r = block_->actions[id.index];
    r = ActionRec{};
    copy_name(r.name, desc.name);
    r.hash = fnv1a32(desc.name);
    r.kind = desc.kind;
    r.context = static_cast<std::uint8_t>(desc.context.index);
    r.flags = action_flag::kDeclared;
    r.repeat = desc.repeat;
    r.press = to_percent(desc.press);
    r.release = to_percent(desc.release);
    for (int s = 0; s < kMaxSlots; ++s) r.slots[s] = make_slot(desc.bindings[s], block_->caps);
    block_->order[block_->declared++] = static_cast<std::uint8_t>(id.index);
    return true;
}

} // namespace nnh
