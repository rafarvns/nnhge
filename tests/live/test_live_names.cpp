// Live value names (spec 030 T1.1, FR-1): every rule and edge, leaf/group split, hashes.

#include <cstring>

#include "live/live_names.hpp"
#include "nnh/live.hpp"
#include "test.hpp"

using nnh::detail::live::check_name;
using nnh::detail::live::group_depth;
using nnh::detail::live::group_hash;
using nnh::detail::live::group_length;
using nnh::detail::live::in_group;
using nnh::detail::live::leaf;
using Error = nnh::detail::live_name::Error;

namespace {
Error check(const char* s) { return check_name(s, std::strlen(s)); }
} // namespace

NNH_TEST(live_name_accepts_31_bytes_and_refuses_32) {
    CHECK(check("abcdefghijklmnopqrstuvwxyz01234") == Error::None);
    CHECK(check("abcdefghijklmnopqrstuvwxyz012345") == Error::TooLong);
}

NNH_TEST(live_name_accepts_every_character_class) {
    CHECK(check("azAZ09_-") == Error::None);
    CHECK(check("player/jump_2-b") == Error::None);
}

NNH_TEST(live_name_refuses_other_characters) {
    CHECK(check("player.jump") == Error::BadCharacter);
    CHECK(check("player jump") == Error::BadCharacter);
    CHECK(check("caf\xc3\xa9") == Error::BadCharacter);
    CHECK(check("a\\b") == Error::BadCharacter);
}

NNH_TEST(live_name_refuses_empty_parts) {
    CHECK(check("") == Error::EmptyPart);
    CHECK(check("/jump") == Error::EmptyPart);
    CHECK(check("player/") == Error::EmptyPart);
    CHECK(check("fx//spark") == Error::EmptyPart);
    CHECK(check("/") == Error::EmptyPart);
}

NNH_TEST(live_name_allows_three_group_levels_not_four) {
    CHECK(check("a/b/c/d") == Error::None);
    CHECK(check("a/b/c/d/e") == Error::TooDeep);
}

NNH_TEST(live_name_checks_length_before_characters) {
    CHECK(check("..............................................") == Error::TooLong);
}

NNH_TEST(live_name_nul_terminated_check_stops_after_32_bytes) {
    CHECK(check_name("abcdefghijklmnopqrstuvwxyz0123456789") == Error::TooLong);
    CHECK(check_name("player/jump") == Error::None);
    CHECK(check_name(nullptr) == Error::EmptyPart);
}

NNH_TEST(live_name_literal_is_hashed_like_fnv1a32) {
    constexpr nnh::live::Name n = "player/jump";
    static_assert(n.length == 11);
    CHECK(n.hash == nnh::detail::fnv1a32("player/jump"));
    CHECK(std::strcmp(n.text, "player/jump") == 0);
}

NNH_TEST(live_name_runtime_copies_nothing_and_measures) {
    char buffer[40] = "goblin/speed";
    const nnh::live::Name n = nnh::live::Name::runtime(buffer);
    CHECK(n.text == buffer);
    CHECK(n.length == 12);
    CHECK(n.hash == nnh::detail::fnv1a32("goblin/speed"));
}

NNH_TEST(live_name_runtime_marks_too_long_names) {
    const nnh::live::Name n = nnh::live::Name::runtime("abcdefghijklmnopqrstuvwxyz0123456789");
    CHECK(n.length == 32);
}

NNH_TEST(live_name_group_depth_counts_slashes) {
    CHECK(group_depth("jump", 4) == 0);
    CHECK(group_depth("player/jump", 11) == 1);
    CHECK(group_depth("a/b/c/d", 7) == 3);
}

NNH_TEST(live_name_group_paths_by_level) {
    const char* s = "fx/spark/big";
    CHECK(group_length(s, 12, 0) == 2);
    CHECK(group_length(s, 12, 1) == 8);
    CHECK(group_length(s, 12, 2) == 0);
    CHECK(group_hash(s, 12, 0) == nnh::detail::fnv1a32("fx"));
    CHECK(group_hash(s, 12, 1) == nnh::detail::fnv1a32("fx/spark"));
    CHECK(group_hash(s, 12, 2) == 0);
    CHECK(group_hash("jump", 4, 0) == 0);
}

NNH_TEST(live_name_leaf_is_after_the_last_slash) {
    CHECK(std::strcmp(leaf("fx/spark/big", 12), "big") == 0);
    CHECK(std::strcmp(leaf("jump", 4), "jump") == 0);
}

NNH_TEST(live_name_in_group_matches_whole_parts_only) {
    CHECK(in_group("fx/spark/big", 12, "fx", 2));
    CHECK(in_group("fx/spark/big", 12, "fx/spark", 8));
    CHECK(!in_group("fx/spark/big", 12, "fx/s", 4));
    CHECK(!in_group("fx/spark/big", 12, "fx/spark/big", 12));
    CHECK(!in_group("fxa/b", 5, "fx", 2));
    CHECK(!in_group("fx", 2, "fx", 2));
}
