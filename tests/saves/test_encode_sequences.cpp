// Encoding arrays and lists (spec 027 T3.2, FR-3, FR-4, FR-8): packed numbers, string sequences with their byte
// count, struct sequences as one template and rows, nested arrays of structs, an empty list's template, and
// max_size<T>() equal to the encoded size of a value with every string and list full (10 shapes).

#include <cstdio>

#include "saves_fixture.hpp"

using saves_test::Bytes;
using saves_test::encoded;
using saves_test::header;
using saves_test::operator+;
using saves_test::Pair;
using saves_test::Seqs;

namespace {

struct Bag {
    Pair cells[2];
};
NNH_SAVE_STRUCT(Bag, NNH_SAVE_FIELD(cells, 1));
struct Shelf {
    nnh::data::List<Bag, 2> bags;
};
NNH_SAVE_TYPE(Shelf, "shelf", NNH_SAVE_FIELD(bags, 1));

struct Wide {
    std::uint64_t big[3] = {};
    nnh::data::List<nnh::Vec2, 5> points;
    nnh::data::List<nnh::Color, 3> colors;
    nnh::data::List<nnh::data::TextureRef, 2> icons;
    double times[2] = {};
    std::int8_t small[4] = {};
};
NNH_SAVE_TYPE(Wide, "wide", NNH_SAVE_FIELD(big, 1), NNH_SAVE_FIELD(points, 2), NNH_SAVE_FIELD(colors, 3),
              NNH_SAVE_FIELD(icons, 4), NNH_SAVE_FIELD(times, 5), NNH_SAVE_FIELD(small, 6));

struct LongText {
    nnh::data::List<nnh::data::String<255>, 255> lines; // 65 282 bytes: under 64 KiB
};
NNH_SAVE_TYPE(LongText, "long_text", NNH_SAVE_FIELD(lines, 40000));

struct ManyPairs {
    nnh::data::List<Pair, 200> pairs;
    bool flags[300] = {};
};
NNH_SAVE_TYPE(ManyPairs, "many", NNH_SAVE_FIELD(pairs, 200), NNH_SAVE_FIELD(flags, 3));

struct Level3 {
    nnh::data::String<5> tag;
};
NNH_SAVE_STRUCT(Level3, NNH_SAVE_FIELD(tag, 1));
struct Level2 {
    Level3 inner[3];
    nnh::data::List<Level3, 2> more;
};
NNH_SAVE_STRUCT(Level2, NNH_SAVE_FIELD(inner, 1), NNH_SAVE_FIELD(more, 2));
struct Level1 {
    Level2 a;
    nnh::data::List<Level2, 3> b;
};
NNH_SAVE_TYPE(Level1, "levels", NNH_SAVE_FIELD(a, 1), NNH_SAVE_FIELD(b, 2));

struct Flat {
    std::uint32_t a = 0;
    float b = 0;
};
NNH_SAVE_TYPE(Flat, "flat", NNH_SAVE_FIELD(a, 1), NNH_SAVE_FIELD(b, 2));

struct BigIds {
    Pair p;
    std::uint16_t q[2] = {};
};
NNH_SAVE_TYPE(BigIds, "big_ids", NNH_SAVE_FIELD(p, 65535), NNH_SAVE_FIELD(q, 200));

struct Sentences {
    nnh::data::List<nnh::data::String<100>, 200> items; // 20 200 bytes of strings: a three-byte length
};
NNH_SAVE_TYPE(Sentences, "sentences", NNH_SAVE_FIELD(items, 1));


template <int N> nnh::data::String<N> full_text(char c) {
    char text[N + 1];
    for (int i = 0; i < N; ++i) text[i] = c;
    text[N] = '\0';
    nnh::data::String<N> s;
    s.assign(text);
    return s;
}

template <class T, int N> void fill(nnh::data::List<T, N>& list, const T& value) {
    while (list.push_back(value)) {}
}

template <class T> bool encodes_to_its_max_size(const T& value, const char* what) {
    const std::size_t size = encoded(value).size();
    if (size != nnh::save::max_size<T>()) {
        std::printf("    %s: %u bytes, max_size %u\n", what, static_cast<unsigned>(size),
                    static_cast<unsigned>(nnh::save::max_size<T>()));
    }
    return size == nnh::save::max_size<T>();
}

} // namespace

NNH_TEST(save_encode_packs_sequences_by_hand) {
    const Bytes expected = header("seqs", 1) + Bytes{
        0x24, 1, 3, 0x01, 0x00, 0x03, 0x02, 0xFF, 0xFF,          // 1 numbers: u16[3]
        0x41, 2, 3, 0x01, 0x00, 0x01,                            // 2 flags: list of 3 bools
        0x52, 3, 3, 8, 2, 'a', 'b', 0, 3, 'x', 'y', 'z',         // 3 words: 3 strings, 8 bytes
        0x53, 4, 2, 2, 0x02, 1, 0x05, 2, 6, 1, 0xFE, 0xFF, 3, 4, 0, // 4 pairs: template {1 u8, 2 i16}, 6 bytes
        0x33, 5, 2, 2, 0x02, 1, 0x05, 2, 6, 5, 6, 0, 7, 8, 0,    // 5 grid: Pair[2]
    };
    CHECK(encoded(saves_test::seqs_sample()) == expected);
}

NNH_TEST(save_encode_writes_an_empty_list_with_its_template) {
    Seqs s = saves_test::seqs_sample();
    s.pairs.clear();
    s.words.clear();
    s.flags.clear();
    const Bytes expected = header("seqs", 1) + Bytes{
        0x24, 1, 3, 0x01, 0x00, 0x03, 0x02, 0xFF, 0xFF,
        0x41, 2, 0,                                  // no flags
        0x52, 3, 0, 0,                               // no words: count, 0 bytes
        0x53, 4, 0, 2, 0x02, 1, 0x05, 2, 0,          // no pairs: count, template, 0 bytes
        0x33, 5, 2, 2, 0x02, 1, 0x05, 2, 6, 5, 6, 0, 7, 8, 0,
    };
    CHECK(encoded(s) == expected);
}

NNH_TEST(save_encode_nests_arrays_of_structs_in_rows) {
    Shelf s;
    Bag bag;
    bag.cells[0] = {1, 2};
    bag.cells[1] = {3, 4};
    s.bags.push_back(bag);
    const Bytes expected = header("shelf", 1) + Bytes{
        0x53, 1, 1, 1, 0x33, 1,   // 1 bags: 1 row, template {1 Pair[]}
        13,                        //   13 bytes of rows
        2, 2, 0x02, 1, 0x05, 2, 6, //   row 0: cells = 2 items, template {1 u8, 2 i16}, 6 bytes
        1, 2, 0, 3, 4, 0,
    };
    CHECK(encoded(s) == expected);
}

NNH_TEST(save_encode_uses_two_byte_lengths_past_127) {
    ManyPairs m;
    for (int i = 0; i < 50; ++i) m.pairs.push_back({static_cast<std::uint8_t>(i), static_cast<std::int16_t>(-i)});
    const Bytes b = encoded(m);
    // field 3 flags: 0x21 3, count 300 = 0xAC 0x02, 300 bytes; then field 200 pairs: 0x53 0xC8 0x01 50 2 ... 150
    CHECK(b.size() == 16 + 4 + (2 + 2 + 300) + (3 + 1 + 5 + 2 + 150));
    CHECK(b[20] == 0x21 && b[21] == 3 && b[22] == 0xAC && b[23] == 0x02);
    const std::size_t p = 20 + 304;
    CHECK(b[p] == 0x53 && b[p + 1] == 0xC8 && b[p + 2] == 0x01 && b[p + 3] == 50);
    CHECK(b[p + 9] == 0x96 && b[p + 10] == 0x01 && b[p + 11] == 0 && b[p + 14] == 1); // 150 bytes; rows 0, 1
}

NNH_TEST(save_max_size_is_the_size_of_a_full_value_for_ten_shapes) {
    Seqs seqs;
    fill(seqs.flags, true);
    fill(seqs.words, full_text<4>('w'));
    fill(seqs.pairs, Pair{});
    CHECK(encodes_to_its_max_size(seqs, "Seqs"));
    saves_test::Kinds kinds;
    kinds.name = full_text<8>('k');
    CHECK(encodes_to_its_max_size(kinds, "Kinds"));
    Shelf shelf;
    fill(shelf.bags, Bag{});
    CHECK(encodes_to_its_max_size(shelf, "Shelf"));
    Wide wide;
    fill(wide.points, nnh::Vec2{1, 2});
    fill(wide.colors, nnh::Color{});
    fill(wide.icons, nnh::data::TextureRef{});
    CHECK(encodes_to_its_max_size(wide, "Wide"));
    static LongText text;
    fill(text.lines, full_text<255>('t'));
    CHECK(encodes_to_its_max_size(text, "LongText"));
    ManyPairs many;
    fill(many.pairs, Pair{});
    CHECK(encodes_to_its_max_size(many, "ManyPairs"));
    Level1 levels;
    Level3 l3;
    l3.tag = full_text<5>('z');
    for (Level3& x : levels.a.inner) x = l3;
    fill(levels.a.more, l3);
    fill(levels.b, levels.a);
    CHECK(encodes_to_its_max_size(levels, "Level1"));
    CHECK(encodes_to_its_max_size(Flat{}, "Flat"));
    CHECK(encodes_to_its_max_size(BigIds{}, "BigIds"));
    static Sentences sentences;
    fill(sentences.items, full_text<100>('s'));
    CHECK(encodes_to_its_max_size(sentences, "Sentences"));
    CHECK(encoded(Shelf{}).size() < nnh::save::max_size<Shelf>()); // an empty list is smaller
}
