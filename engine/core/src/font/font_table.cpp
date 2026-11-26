// The font table (font_table.hpp).

#include "font/font_table.hpp"

namespace nnh::detail {
namespace {

struct Entry {
    std::uint32_t path_hash = 0;
    std::uint32_t generation = 1;
    int refs = 0;
    FontBlock* block = nullptr;
    FontEntryExtra extra{};
};

Entry g_entries[kFontSlots];

bool live(int slot) { return slot >= 0 && slot < kFontSlots && g_entries[slot].refs > 0; }

} // namespace

void font_table_reset() {
    for (Entry& e : g_entries) {
        const std::uint32_t generation = e.generation + 1;
        e = Entry{};
        e.generation = generation;
    }
}

int font_table_find(std::uint32_t path_hash) {
    for (int i = 0; i < kFontSlots; ++i)
        if (g_entries[i].refs > 0 && g_entries[i].path_hash == path_hash) return i;
    return -1;
}

int font_table_add(std::uint32_t path_hash, FontBlock* block) {
    for (int i = 0; i < kFontSlots; ++i) {
        Entry& e = g_entries[i];
        if (e.refs > 0) continue;
        e.path_hash = path_hash;
        e.refs = 1;
        e.block = block;
        e.extra = FontEntryExtra{};
        return i;
    }
    return -1;
}

int font_table_retain(int slot) { return live(slot) ? ++g_entries[slot].refs : 0; }

FontBlock* font_table_release(int slot) {
    if (!live(slot)) return nullptr;
    Entry& e = g_entries[slot];
    if (--e.refs > 0) return nullptr;
    FontBlock* block = e.block;
    const std::uint32_t generation = e.generation + 1;
    e = Entry{};
    e.generation = generation;
    return block;
}

std::uint32_t font_table_id(int slot) {
    return live(slot) ? (g_entries[slot].generation << 6) | static_cast<std::uint32_t>(slot + 1) : 0;
}

int font_table_slot(std::uint32_t id) {
    const int slot = static_cast<int>(id & 63u) - 1;
    if (!live(slot) || (g_entries[slot].generation & 0x03FFFFFFu) != (id >> 6)) return -1;
    return slot;
}

FontBlock* font_table_block(std::uint32_t id) {
    const int slot = font_table_slot(id);
    return slot >= 0 ? g_entries[slot].block : nullptr;
}

int font_table_count() {
    int n = 0;
    for (const Entry& e : g_entries) n += e.refs > 0 ? 1 : 0;
    return n;
}

void font_table_forget(int slot) {
    if (!live(slot)) return;
    const std::uint32_t generation = g_entries[slot].generation + 1;
    g_entries[slot] = Entry{};
    g_entries[slot].generation = generation;
}

FontEntryExtra& font_table_extra(int slot) { return g_entries[slot >= 0 && slot < kFontSlots ? slot : 0].extra; }

} // namespace nnh::detail
