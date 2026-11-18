// Asset path rules (assets/asset_path.hpp, spec 007 FR-6): relative, '/'-separated, no dot segments,
// a portable character set, checked before anything touches the disk.

#include <cstring>

#include "assets/asset_path.hpp"
#include "test.hpp"

using nnh::detail::kMaxAssetPath;
using nnh::detail::valid_asset_path;

NNH_TEST(asset_paths_that_are_relative_and_portable_are_valid) {
    CHECK(valid_asset_path("hero.png"));
    CHECK(valid_asset_path("levels/level1.txt"));
    CHECK(valid_asset_path("art/My Hero-2_final.png"));
    CHECK(valid_asset_path("a/b/c/d/e.bin"));
    CHECK(valid_asset_path("no_extension"));
}

NNH_TEST(empty_or_missing_asset_paths_are_invalid) {
    CHECK(!valid_asset_path(nullptr));
    CHECK(!valid_asset_path(""));
}

NNH_TEST(absolute_and_windows_style_asset_paths_are_invalid) {
    CHECK(!valid_asset_path("/hero.png"));
    CHECK(!valid_asset_path("art\\hero.png"));
    CHECK(!valid_asset_path("C:/hero.png"));
    CHECK(!valid_asset_path("romfs:/hero.png"));
}

NNH_TEST(asset_paths_with_dot_or_empty_segments_are_invalid) {
    CHECK(!valid_asset_path("."));
    CHECK(!valid_asset_path(".."));
    CHECK(!valid_asset_path("./hero.png"));
    CHECK(!valid_asset_path("art/../secret.txt"));
    CHECK(!valid_asset_path("art/.."));
    CHECK(!valid_asset_path("art//hero.png"));
    CHECK(!valid_asset_path("art/"));
}

NNH_TEST(dots_inside_names_are_fine) {
    CHECK(valid_asset_path("hero.v2.png"));
    CHECK(valid_asset_path(".hidden"));
    CHECK(valid_asset_path("art/..png")); // a file name, not a ".." segment
}

NNH_TEST(asset_paths_outside_the_portable_character_set_are_invalid) {
    CHECK(!valid_asset_path("ação.txt"));
    CHECK(!valid_asset_path("tab\there.txt"));
    CHECK(!valid_asset_path("what?.txt"));
    CHECK(!valid_asset_path("star*.png"));
}

NNH_TEST(asset_paths_longer_than_the_limit_are_invalid) {
    char path[kMaxAssetPath + 2];
    std::memset(path, 'a', sizeof path);
    path[kMaxAssetPath] = '\0';
    CHECK(valid_asset_path(path)); // exactly the limit
    path[kMaxAssetPath] = 'a';
    path[kMaxAssetPath + 1] = '\0';
    CHECK(!valid_asset_path(path));
}
