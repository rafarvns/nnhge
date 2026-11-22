// Value types of data assets (spec 014 FR-2): data::String<N> and data::List<T, N> are plain, trivially
// copyable values with fixed capacity, so loaded structs can be copied, stored in arrays and saved like any
// struct.

#include <cstring>
#include <type_traits>

#include "nnh/data.hpp"
#include "test.hpp"

using nnh::data::List;
using nnh::data::String;

static_assert(std::is_trivially_copyable_v<String<24>> && std::is_standard_layout_v<String<24>>);
static_assert(std::is_trivially_copyable_v<List<int, 4>> && std::is_standard_layout_v<List<int, 4>>);
static_assert(sizeof(String<24>) == 25);
static_assert(List<float, 7>::capacity() == 7);

NNH_TEST(data_string_starts_empty) {
    const String<8> s;
    CHECK(s.empty());
    CHECK(s.size() == 0);
    CHECK(std::strcmp(s.c_str(), "") == 0);
}

NNH_TEST(data_string_assign_keeps_text_that_fits) {
    String<5> s;
    CHECK(s.assign("hello"));
    CHECK(s.size() == 5);
    CHECK(s == "hello");
    CHECK(!s.empty());
}

NNH_TEST(data_string_assign_truncates_too_long_text) {
    String<4> s;
    CHECK(!s.assign("goblin"));
    CHECK(s == "gobl");
}

NNH_TEST(data_string_assign_truncates_at_a_code_point) {
    String<4> s;
    CHECK(!s.assign("abc\xC3\xA7")); // "abcç": the 2-byte ç doesn't fit in the 4th byte
    CHECK(s == "abc");
    CHECK(s.size() == 3);
    String<5> t;
    CHECK(!t.assign("ab\xE2\x82\xAC!")); // "ab€!" is 6 bytes; the 3-byte € fits, the ! doesn't
    CHECK(t == "ab\xE2\x82\xAC");
}

NNH_TEST(data_string_assign_null_clears) {
    String<4> s;
    CHECK(s.assign("abcd"));
    CHECK(s.assign(nullptr));
    CHECK(s.empty());
}

NNH_TEST(data_string_assign_clears_the_old_tail) {
    String<6> a;
    String<6> b;
    CHECK(a.assign("abcdef"));
    CHECK(a.assign("xy"));
    CHECK(b.assign("xy"));
    CHECK(std::memcmp(&a, &b, sizeof a) == 0); // equal strings are equal bytes
}

NNH_TEST(data_strings_compare_by_text) {
    String<8> a;
    String<8> b;
    CHECK(a == b);
    CHECK(a.assign("imp"));
    CHECK(!(a == b));
    CHECK(b.assign("imp"));
    CHECK(a == b);
    CHECK(a == "imp");
    CHECK(!(a == "im"));
    CHECK(!(a == "impa"));
}

NNH_TEST(data_list_starts_empty_with_default_items) {
    struct Drop {
        int chance = 7;
    };
    const List<Drop, 3> list;
    CHECK(list.empty());
    CHECK(list.size() == 0);
    CHECK(list.begin() == list.end());
}

NNH_TEST(data_list_push_back_fills_up_to_capacity) {
    List<int, 2> list;
    CHECK(list.push_back(4));
    CHECK(list.push_back(5));
    CHECK(!list.push_back(6));
    CHECK(list.size() == 2);
    CHECK(list[0] == 4);
    CHECK(list[1] == 5);
    int sum = 0;
    for (int v : list) sum += v;
    CHECK(sum == 9);
}

NNH_TEST(data_list_clear_restores_default_items) {
    struct Drop {
        int chance = 7;
    };
    List<Drop, 2> a;
    CHECK(a.push_back(Drop{1}));
    CHECK(a.push_back(Drop{2}));
    a.clear();
    CHECK(a.empty());
    CHECK(a.begin()[0].chance == 7); // the slots behind the count, read through the storage
    CHECK(a.begin()[1].chance == 7);
}

NNH_TEST(data_list_items_are_writable_through_the_index) {
    List<int, 3> list;
    CHECK(list.push_back(1));
    list[0] = 9;
    CHECK(list[0] == 9);
    const List<int, 3>& c = list;
    CHECK(c[0] == 9);
}
