// "Did you mean ...?" (spec 014 T5.2, FR-7): the closest name within an edit distance of max(1, length / 3),
// where swapping two neighbouring letters counts as one edit (optimal string alignment).

#include <string>
#include <vector>

#include "suggest.hpp"
#include "test.hpp"

using nnh::assetc::closest;
using nnh::assetc::edit_distance;

NNH_TEST(edit_distance_counts_insertions_deletions_substitutions_and_swaps) {
    CHECK(edit_distance("damage", "damage") == 0);
    CHECK(edit_distance("damgae", "damage") == 1);
    CHECK(edit_distance("Fier", "Fire") == 1);
    CHECK(edit_distance("ab", "ba") == 1);
    CHECK(edit_distance("ca", "abc") == 3); // optimal string alignment: no edit of a swapped pair
    CHECK(edit_distance("hp", "hpp") == 1);
    CHECK(edit_distance("speed", "sped") == 1);
    CHECK(edit_distance("", "abc") == 3);
    CHECK(edit_distance("kitten", "sitting") == 3);
}

NNH_TEST(closest_suggests_the_nearest_name_within_reach) {
    const std::vector<std::string> names = {"name", "hp", "damage", "speed"};
    CHECK(closest("damgae", names) == "damage");
    CHECK(closest("sped", names) == "speed");
    CHECK(closest("hq", names) == "hp");
    CHECK(closest("nam", names) == "name");
    CHECK(closest("damxyz", names).empty()); // 3 edits: a 6-letter name reaches 2
}

NNH_TEST(closest_suggests_nothing_when_every_name_is_far) {
    const std::vector<std::string> names = {"name", "hp", "damage"};
    CHECK(closest("xyz", names).empty());
    CHECK(closest("dmg", names).empty()); // 3 edits from "damage": a 3-letter name reaches 1
    CHECK(closest("anything", {}).empty());
}

NNH_TEST(closest_prefers_the_first_of_equally_near_names) {
    CHECK(closest("Fiee", {"Fire", "Free"}) == "Fire");
    CHECK(closest("fire", {"Ice", "Fire"}) == "Fire");
}
