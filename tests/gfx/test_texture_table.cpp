// Texture slots with generations (gfx/texture_table.hpp).

#include "gfx/texture_table.hpp"
#include "test.hpp"

using nnh::detail::TextureData;
using nnh::detail::TextureSlot;
using nnh::detail::TextureTable;

namespace {

TextureSlot g_slots[3];

TextureData data(std::uintptr_t handle, int w = 16, int h = 16) {
    return TextureData{handle, w, h, static_cast<std::size_t>(w) * h * 4};
}

} // namespace

NNH_TEST(an_added_texture_can_be_looked_up_by_its_handle) {
    TextureTable t;
    t.reset(g_slots, 3);
    std::uint32_t gen = 0;
    const int slot = t.add(data(77, 32, 8), gen);
    CHECK(slot >= 0);
    const TextureData* d = t.get(slot, gen);
    CHECK(d != nullptr && d->platform == 77 && d->width == 32 && d->height == 8);
    CHECK(t.active_count() == 1);
    CHECK(t.total_bytes() == 32u * 8u * 4u);
}

NNH_TEST(a_removed_texture_gives_back_its_data_once) {
    TextureTable t;
    t.reset(g_slots, 3);
    std::uint32_t gen = 0;
    const int slot = t.add(data(5), gen);
    TextureData removed{};
    CHECK(t.remove(slot, gen, removed));
    CHECK(removed.platform == 5);
    CHECK(!t.remove(slot, gen, removed));
    CHECK(t.get(slot, gen) == nullptr);
    CHECK(t.active_count() == 0 && t.total_bytes() == 0);
}

NNH_TEST(a_stale_handle_does_not_reach_a_reused_slot) {
    TextureTable t;
    t.reset(g_slots, 3);
    std::uint32_t old_gen = 0;
    const int slot = t.add(data(1), old_gen);
    TextureData removed{};
    t.remove(slot, old_gen, removed);
    std::uint32_t new_gen = 0;
    CHECK(t.add(data(2), new_gen) == slot);
    CHECK(new_gen != old_gen);
    CHECK(t.get(slot, old_gen) == nullptr);
    CHECK(!t.remove(slot, old_gen, removed));
    CHECK(t.get(slot, new_gen)->platform == 2);
}

NNH_TEST(a_full_texture_table_refuses_new_textures) {
    TextureTable t;
    t.reset(g_slots, 3);
    std::uint32_t gen = 0;
    for (int i = 0; i < 3; ++i) CHECK(t.add(data(10 + i), gen) >= 0);
    CHECK(t.add(data(99), gen) == -1);
}

NNH_TEST(invalid_slots_are_rejected) {
    TextureTable t;
    t.reset(g_slots, 3);
    TextureData removed{};
    CHECK(t.get(-1, 1) == nullptr);
    CHECK(t.get(3, 1) == nullptr);
    CHECK(!t.remove(7, 1, removed));
}

NNH_TEST(handles_from_a_previous_table_stay_stale_after_reset) {
    TextureTable t;
    t.reset(g_slots, 3);
    std::uint32_t old_gen = 0;
    const int slot = t.add(data(1), old_gen);
    t.reset(g_slots, 3, t.next_generation());
    std::uint32_t new_gen = 0;
    CHECK(t.add(data(2), new_gen) == slot);
    CHECK(t.get(slot, old_gen) == nullptr);
}

NNH_TEST(for_each_active_visits_live_textures) {
    TextureTable t;
    t.reset(g_slots, 3);
    std::uint32_t gen1 = 0;
    std::uint32_t gen2 = 0;
    std::uint32_t gen3 = 0;
    t.add(data(1), gen1);
    const int second = t.add(data(2), gen2);
    t.add(data(3), gen3);
    TextureData removed{};
    CHECK(t.remove(second, gen2, removed));
    std::uintptr_t sum = 0;
    t.for_each_active([&sum](const TextureData& d) { sum += d.platform; });
    CHECK(sum == 4);
}
