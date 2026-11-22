// Converting sections and blocks (spec 014 T5.2, FR-6): nested structs as [a] / [a.b], lists and arrays of
// structs as [[a]] blocks, lists of vectors as nested arrays, and every mismatch between what the file writes
// and what the field is.

#include <string>

#include "convert_fixture.hpp"
#include "data/schema_registry.hpp"
#include "nnh/data.hpp"
#include "test.hpp"

using convert_test::convert;
using convert_test::load_compiled;
using nnh::detail::FieldInfo;
using nnh::detail::TypeInfo;
namespace data = nnh::data;

namespace {

struct Inner {
    std::int16_t n = 0;
};
struct Middle {
    Inner inner;
    bool on = false;
    std::int8_t cap = 0;
};
struct Wave {
    Middle middle;
    data::List<nnh::Vec2, 3> path;
    data::List<data::String<8>, 2> tags;
    Inner pair[2];
    data::List<data::Ref<Wave>, 2> next;
};

struct InnerFields {
    using Self = Inner;
    static constexpr FieldInfo list[] = {NNH_DATA_FIELD(n, .required = true)};
};
struct MiddleFields {
    using Self = Middle;
    static constexpr FieldInfo list[] = {NNH_DATA_FIELD(inner), NNH_DATA_FIELD(on),
                                         NNH_DATA_FIELD(cap, .max = 10)};
};
struct WaveFields {
    using Self = Wave;
    static constexpr FieldInfo list[] = {NNH_DATA_FIELD(middle), NNH_DATA_FIELD(path), NNH_DATA_FIELD(tags),
                                         NNH_DATA_FIELD(pair), NNH_DATA_FIELD(next)};
};
constexpr TypeInfo kInner = nnh::detail::make_type<Inner>("Inner", "", "w.cpp", InnerFields::list);
constexpr TypeInfo kMiddle = nnh::detail::make_type<Middle>("Middle", "", "w.cpp", MiddleFields::list);
constexpr TypeInfo kWave = nnh::detail::make_type<Wave>("Wave", "wave", "w.cpp", WaveFields::list);

struct Registry {
    nnh::detail::DataRegistry registry;
    nnh::detail::TypeNode nodes[3] = {{&kInner, nullptr}, {&kMiddle, nullptr}, {&kWave, nullptr}};
    Registry() {
        for (auto& n : nodes)
            add_type(registry, n);
    }
};

const Registry& local() {
    static const Registry r;
    return r;
}

std::string problems(const std::string& text) {
    const auto r = convert(text, "wave", convert_test::schema_of(local().registry));
    return r.ok ? "ok" : r.problems;
}

const char kPair[] = "[[pair]]\nn = 1\n[[pair]]\nn = 2\n";

} // namespace

NNH_TEST(data_convert_reads_nested_sections_blocks_and_nested_arrays) {
    const std::string text = "path = [[0, 1], [2.5, -3]]\ntags = [\"a\", \"bb\"]\nnext = [\"waves/two\"]\n"
                             "[middle]\non = true\n[middle.inner]\nn = -7\n" +
                             std::string(kPair);
    const auto r = convert(text, "wave", convert_test::schema_of(local().registry));
    CHECK(r.ok);
    Wave w;
    CHECK(load_compiled(r.data.bytes, w, local().registry));
    CHECK(w.middle.on && w.middle.inner.n == -7);
    CHECK(w.path.size() == 2 && w.path.begin()[1].x == 2.5f && w.path.begin()[1].y == -3);
    CHECK(w.tags.size() == 2 && w.tags.begin()[1] == "bb");
    CHECK(w.pair[0].n == 1 && w.pair[1].n == 2);
    CHECK(w.next.size() == 1 && !w.next.begin()[0].empty());
    CHECK(r.data.references.size() == 1 && r.data.references[0].field == "next[0]" &&
          r.data.references[0].target == "wave");
}

NNH_TEST(data_convert_needs_exactly_n_blocks_for_an_array_of_structs) {
    CHECK(problems(kPair) == "ok");
    CHECK(problems("[[pair]]\nn = 1\n") == "f:1: pair: 1 block, exactly 2\n");
    CHECK(problems("") == "ok"); // not written: keeps its defaults
}

NNH_TEST(data_convert_checks_required_fields_in_every_block) {
    CHECK(problems("[[pair]]\nn = 1\n[[pair]]\n") == "f:3: [[pair]]: missing required field 'n'\n");
    CHECK(problems(std::string(kPair) + "[middle.inner]\n") ==
          "f:5: [middle.inner]: missing required field 'n'\n");
}

NNH_TEST(data_convert_refuses_sections_where_values_belong_and_the_reverse) {
    CHECK(problems("middle = 1\n") == "f:1: middle: expected a section ([middle]), found an integer\n");
    CHECK(problems("[[middle]]\n") ==
          "f:1: middle: expected a section ([middle]), found [[middle]] blocks\n");
    CHECK(problems("[pair]\nn = 1\n") == "f:1: pair: expected [[pair]] blocks, found a section ([pair])\n");
    CHECK(problems("pair = [1, 2]\n") == "f:1: pair: expected [[pair]] blocks, found an array\n");
    CHECK(problems("[path]\n") == "f:1: path: expected an array, found a section ([path])\n");
    CHECK(problems("[[tags]]\n") == "f:1: tags: expected an array, found [[tags]] blocks\n");
    CHECK(problems("[middle.on]\n") ==
          "f:1: middle.on: expected true or false, found a section ([middle.on])\n");
}

NNH_TEST(data_convert_checks_items_of_lists) {
    CHECK(problems("path = [[0, 1], [2]]\n") == "f:1: path[1]: expected [x, y]\n");
    CHECK(problems("tags = [\"a\", \"abcdefghi\"]\n") == "f:1: tags[1]: 9 bytes, at most 8\n");
    CHECK(problems("tags = [\"a\", \"b\", \"c\"]\n") == "f:1: tags: 3 items, at most 2\n");
    CHECK(problems("next = [\"\", \"waves/x\"]\n") == "ok");
}

NNH_TEST(data_convert_reports_unknown_sections) {
    CHECK(problems("[midle]\n") == "f:1: unknown field 'midle' (did you mean 'middle'?)\n");
    CHECK(problems("[middle.outer]\nn = 1\n") == "f:1: unknown field 'outer' in [middle]\n");
    CHECK(problems("[middle.innr]\nn = 1\n") ==
          "f:1: unknown field 'innr' in [middle] (did you mean 'inner'?)\n");
}

NNH_TEST(data_convert_names_only_the_bounds_a_field_has) {
    CHECK(problems("[middle]\ncap = 11\n") == "f:2: middle.cap: 11 is out of range: at most 10\n");
    CHECK(problems("[middle]\ncap = -128\n") == "ok");
}

NNH_TEST(data_convert_reports_a_missing_field_at_the_sections_own_header) {
    // [middle.inner] is implied on line 1 and defined on line 2: the problem is reported where it is defined.
    CHECK(problems("[middle.inner.zzz]\n[middle.inner]\n") ==
          "f:1: unknown field 'zzz' in [middle.inner]\nf:2: [middle.inner]: missing required field 'n'\n");
}
