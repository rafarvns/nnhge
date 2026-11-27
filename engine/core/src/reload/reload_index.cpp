// The asset-index kind (see reload_index.hpp).

#include "reload/reload_index.hpp"

#include <new>

#include "assets/asset_index.hpp"
#include "assets/assets_internal.hpp"
#include "nnh/memory.hpp"
#include "reload/reload_assets.hpp"
#include "reload/reload_files.hpp"
#include "reload/reload_memory.hpp"
#include "reload/reload_overlay.hpp"

namespace nnh::detail::reload {
namespace {

/// One block: the AssetIndex, its entries, then the text (parsed in place).
struct IndexBlock {
    AssetIndex index;
};

IndexBlock* g_current = nullptr;  // the batch index lookups use (null: the original)
IndexBlock* g_prepared = nullptr;

void free_block(IndexBlock*& block) {
    if (block == nullptr) return;
    block->~IndexBlock();
    mem::free(block);
    block = nullptr;
}

const char* resolve(void*, const char* old) {
    return g_prepared != nullptr ? g_prepared->index.find(old) : nullptr;
}

} // namespace

Prepared index_prepare(const BatchFile& file) {
    using remote::ReloadReason;
    using remote::ReloadStage;
    if (file.size > kMaxAssetIndexBytes) return prepared_refused(ReloadStage::Validate, ReloadReason::Corrupted);
    // Lines can't outnumber bytes / 2 + 1; the exact count is taken from the text once read.
    const std::size_t text_at = sizeof(IndexBlock);
    void* raw = mem::alloc(MemRegion::Heap, reload_memory_tag(), text_at + file.size + 1);
    if (raw == nullptr) return prepared_refused(ReloadStage::Memory, ReloadReason::NoMemory, file.size + 1);
    auto* block = new (raw) IndexBlock{};
    char* text = static_cast<char*>(raw) + text_at;
    std::size_t read = 0;
    const AssetRedirect r{file.path, file.file, file.size};
    if (file.size > 0 && (!reload_read_file(r, text, read) || read != file.size)) {
        free_block(block);
        return prepared_refused(ReloadStage::Transfer, ReloadReason::Write);
    }
    const int lines = AssetIndex::count_lines(text, file.size);
    const std::size_t entries_bytes = sizeof(const char*) * static_cast<std::size_t>(lines > 0 ? lines : 1);
    // The entries go in a second block of their own (their count is known only now).
    auto* entries = static_cast<const char**>(mem::alloc(MemRegion::Heap, reload_memory_tag(), entries_bytes));
    if (entries == nullptr) {
        free_block(block);
        return prepared_refused(ReloadStage::Memory, ReloadReason::NoMemory, static_cast<std::uint32_t>(entries_bytes));
    }
    if (!block->index.parse(text, file.size, entries, lines)) {
        mem::free(entries);
        free_block(block);
        return prepared_refused(ReloadStage::Validate, ReloadReason::Corrupted);
    }
    g_prepared = block;
    return prepared_ok(block);
}

const AssetIndex* prepared_index() { return g_prepared != nullptr ? &g_prepared->index : nullptr; }

void index_commit(void (*removed)(void* context, std::uint16_t file), void* context) {
    if (g_prepared == nullptr) return;
    if (OverlayTable* o = overlay()) o->repoint(resolve, context, removed);
    IndexBlock* old = g_current;
    g_current = g_prepared;
    g_prepared = nullptr;
    set_current_index(&g_current->index);
    if (old != nullptr) {
        mem::free(const_cast<const char**>(old->index.entries()));
        free_block(old);
    }
}

void index_abandon() {
    if (g_prepared == nullptr) return;
    mem::free(const_cast<const char**>(g_prepared->index.entries()));
    free_block(g_prepared);
}

void index_release() {
    index_abandon();
    if (g_current != nullptr) {
        mem::free(const_cast<const char**>(g_current->index.entries()));
        free_block(g_current);
    }
    set_current_index(nullptr);
}

} // namespace nnh::detail::reload
