// The sprite library kind of hot reload (spec 031 FR-7, FR-8, plan question 5): a reloaded `*.nnhsprite` is validated
// and copied into a new block for every library loaded from it (with the extra capacities it was loaded with) at
// prepare; at commit each library's new block becomes its current one, its identity block (the game's SpriteLibrary)
// forwards to it, the block before is kept as `previous` and the one before that is freed. Animators rebind on their
// next use (animator_rebind.cpp). Tests: tests/animation/test_sprite_reload.cpp, test_animator_rebind.cpp.

#include <cstring>

#include "animation/sprite_library_internal.hpp"
#include "nnh/assets.hpp"
#include "reload/reload_kind.hpp"
#include "reload/reload_memory.hpp"

namespace nnh::detail {
namespace {

using reload::Prepared;
using reload::ReloadItem;
using remote::ReloadReason;
using remote::ReloadStage;

struct NewBlock {
    SpriteReloadEntry* entry = nullptr;
    SpriteBlock* block = nullptr;
};
struct PreparedLibraries {
    int count = 0;
    NewBlock items[1];
};

bool claims(const ReloadItem& item) {
    const std::size_t n = std::strlen(item.path);
    return n > 10 && std::strcmp(item.path + n - 10, ".nnhsprite") == 0;
}

void destroy(PreparedLibraries* p) {
    for (int i = 0; i < p->count; ++i) mem::free(p->items[i].block);
    mem::free(p);
}

Prepared prepare(const ReloadItem& item) {
    if (item.op != remote::ReloadOp::Put) return reload::prepared_ok();
    int count = 0;
    SpriteReloadEntry* entries = sprite_reload_entries(count);
    int matches = 0;
    for (int i = 0; i < count; ++i) matches += entries[i].identity != nullptr && entries[i].hash == item.hash ? 1 : 0;
    if (matches == 0) return reload::prepared_ok();
    auto file = assets::load(item.path); // the batch's new bytes
    if (!file) return reload::prepared_refused(ReloadStage::Memory, ReloadReason::NoMemory, item.size);
    const std::size_t bytes = sizeof(PreparedLibraries) + sizeof(NewBlock) * static_cast<std::size_t>(matches - 1);
    auto* p = static_cast<PreparedLibraries*>(mem::alloc(MemRegion::Heap, reload::reload_memory_tag(), bytes));
    if (p == nullptr) return reload::prepared_refused(ReloadStage::Memory, ReloadReason::NoMemory, bytes);
    p->count = 0;
    for (int i = 0; i < count; ++i) {
        SpriteReloadEntry& e = entries[i];
        if (e.identity == nullptr || e.hash != item.hash) continue;
        const int extras[6] = {e.extras[0], e.extras[1], e.extras[2], e.extras[3], e.extras[4], e.extras[5]};
        auto block = sprite_block_from_file(file.value().data(), file.value().size(), extras, e.name);
        if (!block) {
            destroy(p);
            const bool memory = block.error().code == ErrorCode::OutOfMemory;
            return reload::prepared_refused(memory ? ReloadStage::Memory : ReloadStage::Validate,
                                            memory ? ReloadReason::NoMemory : ReloadReason::Corrupted, item.size);
        }
        block.value()->header.serial = e.serial; // the library's lifetime serial: its animators stay live
        p->items[p->count++] = NewBlock{&e, block.value()};
    }
    return reload::prepared_ok(p);
}

void commit(const ReloadItem&, void* state) {
    auto* p = static_cast<PreparedLibraries*>(state);
    if (p == nullptr) return;
    for (int i = 0; i < p->count; ++i) {
        SpriteReloadEntry& e = *p->items[i].entry;
        mem::free(e.previous); // two reloads back: an animator still on it rebinds without reading it
        e.previous = e.current != e.identity ? e.current : nullptr;
        e.current = p->items[i].block;
        e.identity->header.reserved[0] = kSpriteForwarded;
        ++e.epoch;
        ++g_sprite_reloads;
    }
    mem::free(p);
}

void abandon(const ReloadItem&, void* state) {
    if (auto* p = static_cast<PreparedLibraries*>(state)) destroy(p);
}

reload::ReloadKind g_kind{"sprite library", claims, prepare, commit, abandon};

} // namespace

void sprite_reload_loaded(SpriteBlock* identity, const char* asset_path, const char* name,
                          const LoadSpriteLibraryOptions& options) {
    reload::register_kind(g_kind);
    (void)sprite_reload_track(identity, asset_path, name, options);
}

} // namespace nnh::detail
