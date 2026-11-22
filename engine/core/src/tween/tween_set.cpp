// Tween set storage (spec 015 FR-4): one block per set (tag `tween`), serials, handle validation, move.

#include "nnh/assert.hpp"
#include "tween/tween_internal.hpp"

#include <cstddef>
#include <new>
#include <utility>

NNH_MEM_TAG(tween);

namespace nnh {
namespace detail {
namespace {

std::uint16_t g_last_serial = 0; // handles carry it: a handle of another set is caught

std::uint16_t next_serial() {
    if (++g_last_serial == 0) g_last_serial = 1; // 0 is the empty handle
    return g_last_serial;
}

std::size_t align_up(std::size_t value, std::size_t align) {
    return (value + align - 1) / align * align;
}

bool valid_config(const TweenSetConfig& c) {
    if (c.tweens >= 1 && c.tweens <= kMaxTweens && c.steps >= 0 && c.steps <= kMaxSteps) return true;
    NNH_LOG_ERROR(tween,
                  "TweenSet: capacities {.tweens = %d, .steps = %d} outside 1..%d and 0..%d (no storage)",
                  c.tweens, c.steps, kMaxTweens, kMaxSteps);
    NNH_ASSERT(false, "nnh::TweenSet: invalid capacities");
    return false;
}

TweenBlock* create_block(const TweenSetConfig& c) {
    const std::size_t hot = align_up(sizeof(TweenBlock), alignof(SlotHot));
    const std::size_t cold =
        align_up(hot + sizeof(SlotHot) * static_cast<std::size_t>(c.tweens), alignof(SlotCold));
    const std::size_t steps =
        align_up(cold + sizeof(SlotCold) * static_cast<std::size_t>(c.tweens), alignof(Step));
    const std::size_t total = steps + sizeof(Step) * static_cast<std::size_t>(c.steps);
    auto* bytes = static_cast<unsigned char*>(
        mem::alloc(MemRegion::Heap, tween_memory_tag(), total, alignof(SlotHot) > 8 ? alignof(SlotHot) : 8));
    if (bytes == nullptr) {
        NNH_LOG_ERROR(tween, "TweenSet: out of memory for %zu bytes (no storage)", total);
        return nullptr;
    }
    auto* b = new (bytes) TweenBlock{};
    b->hot = new (bytes + hot) SlotHot[static_cast<std::size_t>(c.tweens)];
    b->cold = new (bytes + cold) SlotCold[static_cast<std::size_t>(c.tweens)];
    b->steps = new (bytes + steps) Step[static_cast<std::size_t>(c.steps)];
    b->capacity = c.tweens;
    b->step_capacity = c.steps;
    b->serial = next_serial();
    for (int i = 0; i < c.tweens; ++i) {
        b->hot[i].next = static_cast<std::int16_t>(i + 1 < c.tweens ? i + 1 : -1);
    }
    b->free_head = 0;
    for (int i = 0; i < c.steps; ++i) b->steps[i].next = static_cast<std::int16_t>(i + 1 < c.steps ? i + 1 : -1);
    b->step_free_head = static_cast<std::int16_t>(c.steps > 0 ? 0 : -1);
    return b;
}

void destroy_block(TweenBlock* b) {
    if (b == nullptr) return;
    if (b->busy == 0) {
        mem::free(b); // slots hold only trivially destructible data: nothing runs, nothing is called back
        return;
    }
    // Misuse: the update or completion running on this block is still on the stack. Kill everything (nothing
    // else is advanced or called back) and let the outermost tween_leave() free the block.
    NNH_LOG_ERROR(tween, "TweenSet destroyed from inside its own update or callback (freed when it returns)");
    NNH_ASSERT(false, "nnh::TweenSet destroyed or replaced from inside its own update or callback");
    for (int i = b->head; i >= 0; i = b->hot[i].next) {
        if (b->hot[i].state == SlotState::Active) tween_kill_slot(*b, i);
    }
    b->doomed = true;
}

} // namespace

MemTag& tween_memory_tag() {
    return NNH_MEM_TAG_REF(tween);
}

int tween_find(const TweenBlock* block, TweenHandle h) {
    if (h.set == 0) return -1; // the empty handle
    if (block == nullptr || h.set != block->serial) {
        NNH_LOG_ERROR(tween, "TweenSet: a handle of another set was used (ignored)");
        NNH_ASSERT(false, "nnh::TweenSet: handle of another set");
        return -1;
    }
    if (h.slot >= block->capacity) return -1;
    const SlotHot& s = block->hot[h.slot];
    return s.state == SlotState::Active && s.generation == h.generation ? h.slot : -1;
}

} // namespace detail

TweenSet::TweenSet(const TweenSetConfig& config) {
    if (detail::valid_config(config)) block_ = detail::create_block(config);
}

TweenSet::~TweenSet() {
    detail::destroy_block(block_);
}

TweenSet::TweenSet(TweenSet&& other) noexcept : block_(std::exchange(other.block_, nullptr)) {}

TweenSet& TweenSet::operator=(TweenSet&& other) noexcept {
    if (this != &other) {
        detail::destroy_block(block_);
        block_ = std::exchange(other.block_, nullptr);
    }
    return *this;
}

int TweenSet::active() const {
    return block_ != nullptr ? block_->active : 0;
}
int TweenSet::capacity() const {
    return block_ != nullptr ? block_->capacity : 0;
}

} // namespace nnh
