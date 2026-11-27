// The font kind of hot reload (spec 031 FR-7, FR-16): the font table already keys its entries by the hash of the load
// path, so a reloaded `<path>.nnhfont` finds its entry with no side table; prepare validates the file and builds a new
// block with new page textures (a full texture table or no memory refuses the batch); commit puts the block in the same
// entry — the same id, so every Font and FontRef keep working with the new metrics and glyphs — and releases the old
// block's pages through D47. The entry's fallbacks stay as loaded. Tests: tests/font/test_font_reload.cpp.

#include <cstring>

#include "font/font_block.hpp"
#include "font/font_format.hpp"
#include "font/font_internal.hpp"
#include "font/font_table.hpp"
#include "nnh/assets.hpp"
#include "nnh/detail/name_hash.hpp"
#include "nnh/memory.hpp"
#include "reload/reload_kind.hpp"
#include "text/font_texture.hpp"

namespace nnh::detail {
namespace {

using reload::Prepared;
using reload::ReloadItem;
using remote::ReloadReason;
using remote::ReloadStage;

constexpr const char* kExtension = ".nnhfont";

/// The table slot loaded from `item`'s path (without ".nnhfont"), or −1.
int slot_of(const ReloadItem& item) {
    const std::size_t n = std::strlen(item.path);
    return font_table_find(fnv1a32(item.path, n - std::strlen(kExtension)));
}

bool claims(const ReloadItem& item) {
    const std::size_t n = std::strlen(item.path), e = std::strlen(kExtension);
    return n > e && std::strcmp(item.path + n - e, kExtension) == 0;
}

struct NewFont {
    int slot = -1;
    FontBlock* block = nullptr;
};

Prepared prepare(const ReloadItem& item) {
    const int slot = item.op == remote::ReloadOp::Put ? slot_of(item) : -1;
    auto file = assets::load(item.path); // the batch's new bytes (validated even when no font uses it)
    if (item.op != remote::ReloadOp::Put) return reload::prepared_ok();
    if (!file) return reload::prepared_refused(ReloadStage::Memory, ReloadReason::NoMemory, item.size);
    const auto* data = static_cast<const std::uint8_t*>(file.value().data());
    if (font::font_validate(data, file.value().size()) != nullptr) {
        return reload::prepared_refused(ReloadStage::Validate, ReloadReason::Corrupted);
    }
    if (slot < 0) return reload::prepared_ok();
    MemTag* tag_ptr = font_table_extra(slot).tag;
    MemTag& tag = tag_ptr != nullptr ? *tag_ptr : font_memory_tag();
    const font::Header h = font::read_header(data);
    const std::size_t bytes = font_block_bytes(h);
    void* memory = mem::alloc(MemRegion::Heap, tag, bytes, 8);
    if (memory == nullptr) return reload::prepared_refused(ReloadStage::Memory, ReloadReason::NoMemory, bytes);
    auto* block = const_cast<FontBlock*>(font_block_init(memory, bytes, data, h.off_page_data));
    for (int i = 0; i < font::kMaxPages; ++i) block->page_slot[i] = -1;
    block->self_slot = static_cast<std::uint8_t>(slot);
    block->name = font_table_extra(slot).path;
    for (int i = 0; i < h.page_count; ++i) {
        ErrorCode code = ErrorCode::None;
        if (font_make_page(*block, i, data, tag, code) != nullptr) {
            font_free_block(block, tag);
            return reload::prepared_refused(ReloadStage::Memory, ReloadReason::NoMemory, item.size);
        }
    }
    auto* p = static_cast<NewFont*>(mem::alloc(MemRegion::Heap, tag, sizeof(NewFont)));
    if (p == nullptr) {
        font_free_block(block, tag);
        return reload::prepared_refused(ReloadStage::Memory, ReloadReason::NoMemory, sizeof(NewFont));
    }
    *p = NewFont{slot, block};
    return reload::prepared_ok(p);
}

MemTag& tag_of(int slot) {
    MemTag* t = font_table_extra(slot).tag;
    return t != nullptr ? *t : font_memory_tag();
}

void commit(const ReloadItem&, void* state) {
    auto* p = static_cast<NewFont*>(state);
    if (p == nullptr) return;
    FontBlock* old = font_table_replace(p->slot, p->block);
    // The old block's pages wait for the GPU (D47); a slot released meanwhile (never expected) takes nothing: the new
    // block is freed instead, so nothing leaks (review nit).
    font_free_block(old != nullptr ? old : p->block, tag_of(p->slot));
    mem::free(p);
}

void abandon(const ReloadItem&, void* state) {
    auto* p = static_cast<NewFont*>(state);
    if (p == nullptr) return;
    font_free_block(p->block, tag_of(p->slot));
    mem::free(p);
}

reload::ReloadKind g_kind{"font", claims, prepare, commit, abandon};

} // namespace

void font_reload_register() { reload::register_kind(g_kind); }

} // namespace nnh::detail
