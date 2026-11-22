// Action set storage (spec 017 FR-1): one block per set (tag `actions`), serial ids, move, destroy; the module's
// log channel and memory tag.

#include "actions/actions_internal.hpp"
#include "nnh/assert.hpp"

#include <cstddef>
#include <new>
#include <utility>

NNH_MEM_TAG(actions);
NNH_LOG_CHANNEL(actions);

namespace nnh {
namespace detail {
namespace {

std::uint16_t g_last_serial = 0;

std::uint16_t next_serial() {
    if (++g_last_serial == 0) g_last_serial = 1; // 0 means "no set"
    return g_last_serial;
}

std::size_t align_up(std::size_t value, std::size_t align) { return (value + align - 1) / align * align; }

bool valid_config(const ActionSetConfig& c) {
    if (c.actions >= 1 && c.actions <= kMaxActions && c.contexts >= 1 && c.contexts <= kMaxContexts) return true;
    NNH_LOG_ERROR(actions, "ActionSet: capacities {.actions = %d, .contexts = %d} outside 1..%d and 1..%d (no storage)",
                  c.actions, c.contexts, kMaxActions, kMaxContexts);
    NNH_ASSERT(false, "nnh::ActionSet: invalid capacities");
    return false;
}

std::uint8_t read_capabilities() {
    std::uint8_t caps = 0;
    for (int i = 0; i < kCapabilityCount; ++i) {
        if (system::has(static_cast<Capability>(i))) caps = static_cast<std::uint8_t>(caps | (1u << i));
    }
    return caps;
}

ActionBlock* create_block(const ActionSetConfig& c) {
    const std::size_t records = align_up(sizeof(ActionBlock), alignof(ActionRec));
    const std::size_t file = records + sizeof(ActionRec) * static_cast<std::size_t>(c.actions);
    const std::size_t total = file + kMaxBindingsSize;
    auto* bytes = static_cast<unsigned char*>(mem::alloc(MemRegion::Heap, actions_memory_tag(), total, 8));
    if (bytes == nullptr) {
        NNH_LOG_ERROR(actions, "ActionSet: out of memory for %zu bytes (no storage)", total);
        return nullptr;
    }
    auto* b = new (bytes) ActionBlock{};
    b->actions = new (bytes + records) ActionRec[static_cast<std::size_t>(c.actions)];
    b->file = bytes + file;
    b->serial = next_serial();
    b->capacity = static_cast<std::uint8_t>(c.actions);
    b->contexts = static_cast<std::uint8_t>(c.contexts);
    b->caps = read_capabilities();
    for (ContextRec& ctx : b->ctx) ctx.block = b;
    ContextRec& def = b->ctx[0];
    def.flags = context_flag::kDeclared | context_flag::kEnabled;
    const char kDefault[] = "default";
    for (std::size_t i = 0; i < sizeof kDefault; ++i) def.name[i] = kDefault[i];
    actions_attach(*b);
    return b;
}

void destroy_block(ActionBlock* b) {
    if (b == nullptr) return;
    bool bound = false;
    for (int i = 0; i < b->contexts; ++i) {
        if (b->ctx[i].scene != nullptr) {
            bound = true;
            context_unbind(*b, i);
        }
    }
    actions_detach(*b);
    mem::free(b); // records hold only trivially destructible data
    if (bound) {
        NNH_LOG_ERROR(actions, "ActionSet destroyed while a context was bound to a scene on the stack (detached)");
        NNH_ASSERT(false, "nnh::ActionSet destroyed while a context is bound to a scene still on the stack");
    }
}

} // namespace

MemTag& actions_memory_tag() { return NNH_MEM_TAG_REF(actions); }

} // namespace detail

ActionSet::ActionSet(const ActionSetConfig& config) {
    if (detail::valid_config(config)) block_ = detail::create_block(config);
}

ActionSet::~ActionSet() { detail::destroy_block(block_); }

ActionSet::ActionSet(ActionSet&& other) noexcept : block_(std::exchange(other.block_, nullptr)) {}

ActionSet& ActionSet::operator=(ActionSet&& other) noexcept {
    if (this != &other) {
        detail::destroy_block(block_);
        block_ = std::exchange(other.block_, nullptr);
    }
    return *this;
}

bool ActionSet::valid() const { return block_ != nullptr; }
std::uint16_t ActionSet::id() const { return block_ != nullptr ? block_->serial : 0; }

} // namespace nnh
