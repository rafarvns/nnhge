// Enum names (spec 014 FR-2): NNH_DATA_ENUM keeps its names as one stringified list ("Normal, Fire, Ice");
// these pure helpers split it and find a name's index. The converter uses them on lists read from a
// description too.

#include <cstring>

#include "data/enum_names.hpp"
#include "test.hpp"

using nnh::detail::enum_index;
using nnh::detail::enum_name;
using nnh::detail::NameView;

namespace {
bool is(NameView v, const char* text) {
    return v.length == static_cast<int>(std::strlen(text)) && std::strncmp(v.text, text, v.length) == 0;
}
} // namespace

NNH_TEST(enum_name_splits_the_list) {
    const char* names = "Normal, Fire, Ice";
    CHECK(is(enum_name(names, 0), "Normal"));
    CHECK(is(enum_name(names, 1), "Fire"));
    CHECK(is(enum_name(names, 2), "Ice"));
}

NNH_TEST(enum_name_out_of_range_is_empty) {
    CHECK(enum_name("A, B", 2).length == 0);
    CHECK(enum_name("A, B", -1).length == 0);
    CHECK(enum_name("", 0).length == 0);
}

NNH_TEST(enum_name_ignores_spacing) {
    const char* names = "A,B ,  C";
    CHECK(is(enum_name(names, 0), "A"));
    CHECK(is(enum_name(names, 1), "B"));
    CHECK(is(enum_name(names, 2), "C"));
}

NNH_TEST(enum_index_finds_whole_names_only) {
    const char* names = "Normal, Fire, Ice";
    CHECK(enum_index(names, "Fire", 4) == 1);
    CHECK(enum_index(names, "Ice", 3) == 2);
    CHECK(enum_index(names, "Normal", 6) == 0);
    CHECK(enum_index(names, "Fir", 3) == -1);
    CHECK(enum_index(names, "fire", 4) == -1); // case-sensitive
    CHECK(enum_index(names, "Fire, Ice", 9) == -1);
    CHECK(enum_index(names, "", 0) == -1);
}

NNH_TEST(enum_index_reads_only_the_given_length) { CHECK(enum_index("Normal, Fire", "Fire = 3", 4) == 1); }
