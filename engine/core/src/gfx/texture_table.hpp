#pragma once
// Fixed table of loaded textures over caller-provided slots. Pure logic; unit tested in
// tests/gfx/test_texture_table.cpp.
//
// A texture is identified by {slot, generation}: removing a slot bumps its generation, so an old
// handle can't reach a texture that reused the slot (same scheme as the event subscriber table).

#include <cstddef>
#include <cstdint>

namespace nnh::detail {

/// What core knows about a texture the platform created.
struct TextureData {
    std::uintptr_t platform = 0; ///< the platform's handle
    int width = 0;
    int height = 0;
    std::size_t bytes = 0; ///< GPU memory it uses
};

struct TextureSlot {
    TextureData data;
    std::uint32_t generation = 1;
    bool active = false;
};

class TextureTable {
public:
    /// Uses `slots` for storage and forgets every texture. Generations start at `first_generation`, so
    /// handles from a previous table (see next_generation()) stay stale.
    void reset(TextureSlot* slots, int capacity, std::uint32_t first_generation = 1);

    /// @return slot index, or -1 if the table is full. `generation_out` identifies this texture.
    int add(const TextureData& data, std::uint32_t& generation_out);

    /// Removes a live texture and copies its data to `removed` (to destroy it on the platform).
    /// @return false if the handle is stale.
    bool remove(int slot, std::uint32_t generation, TextureData& removed);

    /// The texture's data, or nullptr if the handle is stale.
    const TextureData* get(int slot, std::uint32_t generation) const;

    int active_count() const;
    std::size_t total_bytes() const;
    int capacity() const { return capacity_; }

    /// A generation higher than any used so far: pass it to the next reset().
    std::uint32_t next_generation() const;

    template <typename Fn>
    void for_each_active(Fn&& fn) const {
        for (int i = 0; i < capacity_; ++i) {
            if (slots_[i].active) fn(slots_[i].data);
        }
    }

private:
    TextureSlot* slots_ = nullptr;
    int capacity_ = 0;
};

} // namespace nnh::detail
