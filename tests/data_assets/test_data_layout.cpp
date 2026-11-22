// Layout independence (spec 014 FR-10, AC-4): the converted format stores values by schema field, never as a
// memory image, so two structs with the same schema but another member order and padding — as two compilers
// might lay out one struct — share the fingerprint, accept the same converted bytes and load the same values.

#include <cstdint>

#include "data/data_apply.hpp"
#include "data/data_validate.hpp"
#include "data/schema_fingerprint.hpp"
#include "data/schema_registry.hpp"
#include "data_file.hpp"
#include "test.hpp"

using nnh::detail::DataRegistry;
using nnh::detail::FieldInfo;
using nnh::detail::make_type;
using nnh::detail::TypeInfo;

namespace {

struct Packed {
    std::uint8_t small = 1;
    std::int32_t big = 2;
    nnh::data::String<5> name;
    float ratio = 0;
    nnh::data::List<std::int16_t, 3> marks;
};
// The same members, reordered: other offsets, other padding, another size.
struct Shuffled {
    nnh::data::List<std::int16_t, 3> marks;
    float ratio = 0;
    nnh::data::String<5> name;
    std::int32_t big = 2;
    std::uint8_t small = 1;
};

#define NNH_TEST_LAYOUT_FIELDS                                                                               \
    NNH_DATA_FIELD(small), NNH_DATA_FIELD(big), NNH_DATA_FIELD(name), NNH_DATA_FIELD(ratio),                 \
        NNH_DATA_FIELD(marks)
struct PackedFields {
    using Self = Packed;
    static constexpr FieldInfo list[] = {NNH_TEST_LAYOUT_FIELDS};
};
struct ShuffledFields {
    using Self = Shuffled;
    static constexpr FieldInfo list[] = {NNH_TEST_LAYOUT_FIELDS};
};
constexpr TypeInfo kPacked = make_type<Packed>("Thing", "thing", "a.cpp", PackedFields::list);
constexpr TypeInfo kShuffled = make_type<Shuffled>("Thing", "thing", "b.cpp", ShuffledFields::list);

static_assert(offsetof(Packed, big) != offsetof(Shuffled, big) &&
              offsetof(Packed, marks) != offsetof(Shuffled, marks));

std::uint32_t fingerprint(const TypeInfo& t) {
    DataRegistry r;
    nnh::detail::TypeNode node{&t, nullptr};
    add_type(r, node);
    return nnh::detail::schema_fingerprint(r, t);
}

template <class T> T load(const data_file::Bytes& b, const TypeInfo& t) {
    DataRegistry r;
    nnh::detail::TypeNode node{&t, nullptr};
    add_type(r, node);
    T out;
    t.reset(&out);
    CHECK(nnh::detail::validate_data(b.data(), b.size(), r, t, fingerprint(t)).ok());
    nnh::detail::apply_data(b.data(), b.size(), r, t, &out);
    return out;
}

} // namespace

NNH_TEST(data_layout_does_not_change_the_fingerprint) {
    CHECK(fingerprint(kPacked) == fingerprint(kShuffled));
    CHECK(kPacked.fields[1].offset != kShuffled.fields[1].offset);
}

NNH_TEST(data_layout_does_not_change_the_loaded_values) {
    data_file::Writer w(fingerprint(kPacked));
    w.fields(5).field(0).u8(200).field(1).u32(0xFFFFFF85u).field(2).str("kobo").field(3).f32(0.75f);
    w.field(4).u16(2).u16(0xFFFF).u16(12);
    const data_file::Bytes bytes = w.bytes();
    const Packed a = load<Packed>(bytes, kPacked);
    const Shuffled b = load<Shuffled>(bytes, kShuffled);
    CHECK(a.small == 200 && b.small == 200);
    CHECK(a.big == -123 && b.big == -123);
    CHECK(a.name == "kobo" && b.name == "kobo");
    CHECK(a.ratio == 0.75f && b.ratio == 0.75f);
    CHECK(a.marks.size() == 2 && b.marks.size() == 2);
    CHECK(a.marks[0] == -1 && b.marks[0] == -1 && a.marks[1] == 12 && b.marks[1] == 12);
}
