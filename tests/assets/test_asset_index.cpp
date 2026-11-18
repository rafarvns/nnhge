// The asset index written by the build (assets/asset_index.hpp, spec 007 FR-6): sorted paths, looked up
// case-sensitively on every system, with a hint when only the letter case differs.

#include <cstring>

#include "assets/asset_index.hpp"
#include "test.hpp"

using nnh::detail::AssetIndex;

namespace {

char g_text[256];
const char* g_entries[8];

/// Copies `text` into the index's buffer (parse works in place) and parses it.
bool parse(AssetIndex& index, const char* text) {
    const std::size_t size = std::strlen(text);
    std::memcpy(g_text, text, size);
    return index.parse(g_text, size, g_entries, 8);
}

} // namespace

NNH_TEST(index_finds_paths_exactly) {
    AssetIndex index;
    CHECK(parse(index, "levels/level1.txt\nart/hero.png\nart/tiles.png\n"));
    CHECK(index.count() == 3);
    CHECK(std::strcmp(index.find("art/hero.png"), "art/hero.png") == 0);
    CHECK(index.find("levels/level1.txt") != nullptr);
    CHECK(index.find("art/missing.png") == nullptr);
    CHECK(index.find("art") == nullptr); // a folder is not an asset
}

NNH_TEST(index_lookup_is_case_sensitive_with_a_hint) {
    AssetIndex index;
    CHECK(parse(index, "art/Hero.png\n"));
    CHECK(index.find("art/hero.png") == nullptr);
    CHECK(std::strcmp(index.find_ignoring_case("art/hero.png"), "art/Hero.png") == 0);
    CHECK(index.find_ignoring_case("art/villain.png") == nullptr);
}

NNH_TEST(index_accepts_windows_line_endings_and_blank_lines) {
    AssetIndex index;
    CHECK(parse(index, "b.txt\r\n\r\na.txt\r\n"));
    CHECK(index.count() == 2 && index.find("a.txt") != nullptr && index.find("b.txt") != nullptr);
}

NNH_TEST(index_without_a_final_newline_keeps_the_last_path) {
    AssetIndex index;
    CHECK(parse(index, "a.txt\nz.txt"));
    CHECK(index.find("z.txt") != nullptr);
}

NNH_TEST(an_empty_index_has_no_assets) {
    AssetIndex index;
    CHECK(parse(index, ""));
    CHECK(index.count() == 0 && index.find("a.txt") == nullptr);
}

NNH_TEST(index_with_an_invalid_path_is_refused) {
    AssetIndex index;
    CHECK(!parse(index, "ok.txt\n../escape.txt\n"));
    CHECK(index.count() == 0);
}

NNH_TEST(index_with_more_paths_than_room_is_refused) {
    AssetIndex index;
    CHECK(!parse(index, "1\n2\n3\n4\n5\n6\n7\n8\n9\n"));
    CHECK(index.count() == 0);
}

NNH_TEST(index_counts_lines_to_size_its_storage) {
    CHECK(AssetIndex::count_lines("a\nb\r\n\nc", 7) == 3);
    CHECK(AssetIndex::count_lines("", 0) == 0);
}
