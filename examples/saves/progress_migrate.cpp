// The saves example's migration steps (spec 027 FR-12, FR-13): what automatic evolution can't guess. Each runs after
// the automatic load, in order, and reads the file's retired fields through `old`.

#include "progress_history.hpp"

namespace saves_example {
namespace {

/// The v1 weapon (field 4) as the first inventory item, equipped: what step 1 adds to any later version.
template <class ItemT, int N> void weapon_to_inventory(nnh::data::List<ItemT, N>& inventory,
                                                       const nnh::save::OldFields& old) {
    std::uint16_t weapon = 0;
    if (!old.get(nnh::save::path(4), weapon) || weapon == 0) return;
    ItemT first{};
    first.kind = weapon;
    first.equipped = true;
    if (inventory.size() == N) return; // full: the weapon is lost (a step can't fail; documented)
    inventory.push_back(first);
    for (int i = inventory.size() - 1; i > 0; --i) { // move it to the front
        const ItemT moved = inventory[i];
        inventory[i] = inventory[i - 1];
        inventory[i - 1] = moved;
    }
}

} // namespace

void progress_from_v1(Progress& p, const nnh::save::OldFields& old) { weapon_to_inventory(p.inventory, old); }

void progress_from_v2(Progress& p, const nnh::save::OldFields& old) {
    std::uint32_t coins = 0;
    if (old.get(nnh::save::path(3), coins)) p.gems = coins / 10; // coins became gems at 10:1
    std::uint32_t location = 0;
    if (old.get(nnh::save::path(16), location)) { // one u32 split in two (absent in v1 files: the defaults stay)
        p.room = static_cast<std::uint16_t>(location >> 16);
        p.tile = static_cast<std::uint16_t>(location & 0xFFFFu);
    }
}

void progress_v2_from_v1(ProgressV2& p, const nnh::save::OldFields& old) { weapon_to_inventory(p.inventory, old); }

} // namespace saves_example
