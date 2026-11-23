// A world's block layout (spec 020 FR-9, constitution II; T2.1): a pure function of the description. Every part
// aligned, totals by the plan's formula, the 1024 × 8 × 4 example ≈ 64 KB besides the components.

#include "entities/block_layout.hpp"
#include "world_fixture.hpp"

#include <cstdint>

using nnh::ComponentDesc;
using nnh::detail::world_layout;
using nnh::detail::WorldLayout;
using world_test::Hp;
using world_test::Pos;
using world_test::Vel;
using world_test::Wide;

namespace {

bool aligned(std::size_t offset, std::size_t align) { return offset % align == 0; }

} // namespace

NNH_TEST(world_layout_record_holds_meta_kinds_tags_and_a_slot_per_kind) {
    const ComponentDesc kinds[] = {nnh::component<Pos>(8), nnh::component<Vel>(8), nnh::component<Hp>(8)};
    const WorldLayout l = world_layout({.max_entities = 10, .tags_per_entity = 4}, kinds, 3);
    CHECK(l.slots_word == 6);   // meta, kinds, 4 tags
    CHECK(l.record_words == 8); // + 3 u16 slots rounded up to 2 words
    const WorldLayout none = world_layout({.max_entities = 10, .tags_per_entity = 0}, kinds, 2);
    CHECK(none.slots_word == 2 && none.record_words == 3);
}

NNH_TEST(world_layout_aligns_every_part_to_its_type) {
    const ComponentDesc kinds[] = {nnh::component<Hp>(5), nnh::component<Wide>(3), nnh::component<Pos>(7)};
    const WorldLayout l = world_layout({.max_entities = 7, .tags_per_entity = 1}, kinds, 3);
    CHECK(aligned(l.kinds[1].data, 32));
    CHECK(l.align >= 32);
    CHECK(aligned(l.records, 4) && aligned(l.entity_bitmaps, 4) && aligned(l.queue, 2));
    for (int k = 0; k < 3; ++k) {
        CHECK(aligned(l.kinds[k].data, static_cast<std::size_t>(kinds[k].align)));
        CHECK(aligned(l.kinds[k].owner, 2) && aligned(l.kinds[k].bitmaps, 4));
        CHECK(l.kinds[k].data + static_cast<std::size_t>(kinds[k].capacity + 1) * kinds[k].size <= l.total);
    }
    CHECK(l.kinds[1].data + 4 * 32 <= l.kinds[1].owner || l.kinds[1].owner < l.kinds[1].data);
}

NNH_TEST(world_layout_of_one_and_of_thirty_two_kinds) {
    ComponentDesc many[32];
    for (ComponentDesc& d : many) d = nnh::component<Pos>(4);
    const WorldLayout l32 = world_layout({.max_entities = 4, .tags_per_entity = 0}, many, 32);
    CHECK(l32.record_words == 2 + 16);
    const WorldLayout l1 = world_layout({.max_entities = 4, .tags_per_entity = 0}, many, 1);
    CHECK(l1.record_words == 3);
    CHECK(l1.total < l32.total);
}

NNH_TEST(world_layout_of_1024_entities_8_kinds_4_tags_is_about_64_kb_besides_components) {
    ComponentDesc kinds[8];
    for (ComponentDesc& d : kinds) d = nnh::component<Hp>(1024);
    const WorldLayout l = world_layout({.max_entities = 1024, .tags_per_entity = 4}, kinds, 8);
    const std::size_t components = 8 * 1025 * sizeof(Hp);
    const std::size_t bookkeeping = l.total - components;
    // Per entity: (2 + 4 + 4) words of record + 4 bitmap bits + a 2-byte queue link; per component slot 2 B owner and
    // 4 bitmap bits; plus the header, the kind table and the ids.
    CHECK(l.record_words == 10);
    CHECK(bookkeeping > 60 * 1024 && bookkeeping < 66 * 1024);
}
