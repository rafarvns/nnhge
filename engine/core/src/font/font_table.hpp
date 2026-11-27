#pragma once
// The font table (spec 029 D49; plan decision 5): 32 entries, refcounted by the FNV-1a hash of the asset path, so loading
// a path twice shares one block. An entry's id (what FontRef holds) is (generation << 6) | (slot + 1); 0 is the built-in
// font; a released slot's generation moves on, so old ids resolve to nothing. Module state, no allocation.
// Tests: tests/font/test_font_table.cpp.

#include <cstdint>

#include "font/font_block.hpp"

namespace nnh {
struct MemTag;
}

namespace nnh::detail {

inline constexpr int kFontSlots = 32;

/// What the loader keeps per entry besides the block.
struct FontEntryExtra {
    int fallbacks[font::kMaxFallbacks] = {-1, -1, -1}; ///< table slots this entry holds a reference to
    int fallback_count = 0;
    bool fallbacks_loaded = false;  ///< its own fallbacks were loaded (false for an entry loaded only as a fallback)
    MemTag* tag = nullptr;          ///< the block's tag
    char path[font::kPathBytes] = {}; ///< the asset path (logs, Font::path in Debug/Development)
};

void font_table_reset();
/// The slot loaded from `path_hash`, or −1.
int font_table_find(std::uint32_t path_hash);
/// A new entry with one reference. @return its slot, or −1 when the table is full.
int font_table_add(std::uint32_t path_hash, FontBlock* block);
/// One more reference. @return the references now.
int font_table_retain(int slot);
/// One reference less: the last one empties the entry (its generation moves on) and returns its block for the caller
/// to free; otherwise nullptr.
FontBlock* font_table_release(int slot);
std::uint32_t font_table_id(int slot);
/// The live entry's block of `id`, or nullptr (0, stale, garbage).
FontBlock* font_table_block(std::uint32_t id);
/// The slot of a live `id`, or −1.
int font_table_slot(std::uint32_t id);
int font_table_count();
/// Empties an entry without freeing its block (an entry of an earlier engine run, in Release: its textures are gone).
void font_table_forget(int slot);
FontEntryExtra& font_table_extra(int slot);
/// Puts `block` in a live entry (spec 031: a hot reload; the id, references and fallbacks stay). @return the old block.
FontBlock* font_table_replace(int slot, FontBlock* block);

} // namespace nnh::detail
