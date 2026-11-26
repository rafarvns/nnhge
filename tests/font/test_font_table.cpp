// The font table (spec 029 FR-5, FR-35; plan decision 5): 32 entries refcounted by path hash, FontRef = slot +
// generation (0 = the built-in font), stale refs detected after the last release.

#include "font/font_block.hpp"
#include "font/font_table.hpp"
#include "test.hpp"

using namespace nnh::detail;

namespace {

FontBlock g_blocks[40];

} // namespace

NNH_TEST(font_table_adds_finds_and_resolves_entries) {
    font_table_reset();
    const int a = font_table_add(0x1111u, &g_blocks[0]);
    const int b = font_table_add(0x2222u, &g_blocks[1]);
    CHECK(a >= 0 && b >= 0 && a != b);
    CHECK(font_table_find(0x1111u) == a && font_table_find(0x2222u) == b && font_table_find(0x3333u) == -1);
    const std::uint32_t id = font_table_id(a);
    CHECK(id != 0);
    CHECK(font_table_block(id) == &g_blocks[0]);
    CHECK(font_table_slot(id) == a);
    CHECK(font_table_block(0) == nullptr); // the built-in font
}

NNH_TEST(font_table_shares_a_path_by_reference_count) {
    font_table_reset();
    const int a = font_table_add(0x1111u, &g_blocks[0]);
    CHECK(font_table_retain(a) == 2);
    CHECK(font_table_release(a) == nullptr);        // one owner left
    CHECK(font_table_block(font_table_id(a)) == &g_blocks[0]);
    CHECK(font_table_release(a) == &g_blocks[0]);   // the last owner: the caller frees the block
    CHECK(font_table_find(0x1111u) == -1);
}

NNH_TEST(font_table_a_released_slot_makes_old_refs_stale) {
    font_table_reset();
    const int a = font_table_add(0x1111u, &g_blocks[0]);
    const std::uint32_t old_id = font_table_id(a);
    font_table_release(a);
    CHECK(font_table_block(old_id) == nullptr);
    const int again = font_table_add(0x2222u, &g_blocks[1]);
    CHECK(again == a);                             // the lowest free slot is reused
    CHECK(font_table_id(again) != old_id);         // with a new generation
    CHECK(font_table_block(old_id) == nullptr);
}

NNH_TEST(font_table_holds_32_fonts) {
    font_table_reset();
    bool ok = true;
    for (int i = 0; i < 32; ++i) ok = ok && font_table_add(0x100u + static_cast<unsigned>(i), &g_blocks[i]) >= 0;
    CHECK(ok);
    CHECK(font_table_add(0x999u, &g_blocks[33]) == -1);
    CHECK(font_table_count() == 32);
    font_table_reset();
    CHECK(font_table_count() == 0);
}

NNH_TEST(font_table_ids_of_garbage_resolve_to_nothing) {
    font_table_reset();
    font_table_add(0x1111u, &g_blocks[0]);
    CHECK(font_table_block(0xFFFFFFFFu) == nullptr);
    CHECK(font_table_block(63u) == nullptr); // slot 62: out of range
    CHECK(font_table_block(1u) == nullptr);  // slot 0, generation 0: never issued
}
