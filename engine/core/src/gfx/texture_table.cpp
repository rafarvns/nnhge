// Texture table (see texture_table.hpp).

#include "gfx/texture_table.hpp"

namespace nnh::detail {

void TextureTable::reset(TextureSlot* slots, int capacity, std::uint32_t first_generation) {
    slots_ = slots;
    capacity_ = capacity;
    for (int i = 0; i < capacity_; ++i) slots_[i] = TextureSlot{TextureData{}, first_generation, false};
}

int TextureTable::add(const TextureData& data, std::uint32_t& generation_out) {
    for (int i = 0; i < capacity_; ++i) {
        TextureSlot& slot = slots_[i];
        if (slot.active) continue;
        slot.data = data;
        slot.active = true;
        generation_out = slot.generation;
        return i;
    }
    return -1;
}

bool TextureTable::remove(int slot, std::uint32_t generation, TextureData& removed) {
    if (get(slot, generation) == nullptr) return false;
    TextureSlot& s = slots_[slot];
    removed = s.data;
    s.data = TextureData{};
    s.active = false;
    ++s.generation;
    return true;
}

const TextureData* TextureTable::get(int slot, std::uint32_t generation) const {
    if (slot < 0 || slot >= capacity_) return nullptr;
    const TextureSlot& s = slots_[slot];
    return s.active && s.generation == generation ? &s.data : nullptr;
}

int TextureTable::active_count() const {
    int n = 0;
    for (int i = 0; i < capacity_; ++i) n += slots_[i].active ? 1 : 0;
    return n;
}

std::size_t TextureTable::total_bytes() const {
    std::size_t total = 0;
    for (int i = 0; i < capacity_; ++i) total += slots_[i].active ? slots_[i].data.bytes : 0;
    return total;
}

std::uint32_t TextureTable::next_generation() const {
    std::uint32_t highest = 0;
    for (int i = 0; i < capacity_; ++i) highest = slots_[i].generation > highest ? slots_[i].generation : highest;
    return highest + 1;
}

} // namespace nnh::detail
