// Save declarations (spec 027 T2.1, T2.2, FR-1, FR-2, FR-3, FR-8, FR-12): kinds, widths, shapes and capacities
// deduced from member types; the tables of a type, a struct and a summary (fields sorted by id, retired ids, steps,
// version, key, hash, max size) in the global namespace, a game namespace, a header and one source; a struct that is
// also a data type; names stripped in Release.

#include <cstddef>
#include <string_view>

#include "nnh/build.hpp"
#include "nnh/detail/name_hash.hpp"
#include "saves_fixture.hpp"

using nnh::detail::SaveFieldInfo;
using nnh::detail::SaveRole;
using nnh::detail::SaveTypeInfo;
using nnh::save::WireKind;
using nnh::save::WireShape;
using saves_test::Kinds;
using saves_test::Pair;
using saves_test::Seqs;

// ---- Declared in this source only (the "one source" pattern), and in the global namespace ----

struct GlobalSave {
    std::int32_t x = 0;
};
NNH_SAVE_TYPE(GlobalSave, "global", NNH_SAVE_FIELD(x, 1));

struct Versioned {
    std::uint32_t a = 0;
    std::uint32_t b = 0;
};
void versioned_from_v1(Versioned&, const nnh::save::OldFields&) {}
void versioned_from_v2(Versioned&, const nnh::save::OldFields&) {}
NNH_SAVE_TYPE(Versioned, "versioned", NNH_SAVE_FIELD(b, 7), NNH_SAVE_STEP(2, versioned_from_v2), NNH_SAVE_VERSION(3),
              NNH_SAVE_RETIRED(9, 4), NNH_SAVE_FIELD(a, 2), NNH_SAVE_STEP(1, versioned_from_v1), NNH_SAVE_RETIRED(5));

struct VersionedSummary {
    std::uint32_t b = 0;
};
NNH_SAVE_SUMMARY(VersionedSummary, "versioned", 3, NNH_SAVE_FIELD(b, 7));

// A spec 014 data type (registered by tests/data_assets/test_types.cpp) that is also a save type: the two
// declarations don't interact.
namespace test_data {
NNH_SAVE_TYPE(Item, "item", NNH_SAVE_FIELD(name, 1), NNH_SAVE_FIELD(price, 2), NNH_SAVE_FIELD(icon, 3));
} // namespace test_data

namespace {

template <class T> const SaveTypeInfo& info() { return nnh::detail::save_info_of<T>(); }

const SaveFieldInfo& field(const SaveTypeInfo& t, int id) {
    for (int i = 0; i < t.field_count; ++i) {
        if (t.fields[i].id == id) return t.fields[i];
    }
    static const SaveFieldInfo none{};
    return none;
}

bool is(const SaveFieldInfo& f, WireKind kind, WireShape shape, int width, int count) {
    return f.kind == kind && f.shape == shape && f.width == width && f.count == count;
}

} // namespace

NNH_TEST(save_declaration_deduces_every_one_value_kind) {
    const SaveTypeInfo& t = info<Kinds>();
    CHECK(is(field(t, 1), WireKind::Bool, WireShape::One, 1, 1));
    CHECK(is(field(t, 2), WireKind::U8, WireShape::One, 1, 1));
    CHECK(is(field(t, 3), WireKind::I8, WireShape::One, 1, 1));
    CHECK(is(field(t, 4), WireKind::U16, WireShape::One, 2, 1));
    CHECK(is(field(t, 5), WireKind::I16, WireShape::One, 2, 1));
    CHECK(is(field(t, 6), WireKind::U32, WireShape::One, 4, 1));
    CHECK(is(field(t, 7), WireKind::I32, WireShape::One, 4, 1));
    CHECK(is(field(t, 8), WireKind::U64, WireShape::One, 8, 1));
    CHECK(is(field(t, 9), WireKind::I64, WireShape::One, 8, 1));
    CHECK(is(field(t, 10), WireKind::F32, WireShape::One, 4, 1));
    CHECK(is(field(t, 11), WireKind::F64, WireShape::One, 8, 1));
    CHECK(is(field(t, 12), WireKind::Vec2, WireShape::One, 0, 1) && field(t, 12).stride == 8);
    CHECK(is(field(t, 13), WireKind::Color, WireShape::One, 0, 1) && field(t, 13).stride == 4);
    CHECK(is(field(t, 14), WireKind::DataRef, WireShape::One, 0, 1));
    CHECK(is(field(t, 15), WireKind::TextureRef, WireShape::One, 0, 1));
    CHECK(is(field(t, 16), WireKind::SoundRef, WireShape::One, 0, 1));
    CHECK(is(field(t, 17), WireKind::MusicRef, WireShape::One, 0, 1));
    CHECK(is(field(t, 18), WireKind::String, WireShape::One, 0, 1) && field(t, 18).capacity == 8);
    CHECK(is(field(t, 19), WireKind::Struct, WireShape::One, 0, 1) && field(t, 19).nested == &info<Pair>());
    CHECK(field(t, 19).stride == sizeof(Pair) && field(t, 19).offset == offsetof(Kinds, pair));
}

NNH_TEST(save_declaration_stores_enums_by_their_underlying_type) {
    const SaveTypeInfo& t = info<Kinds>();
    CHECK(is(field(t, 20), WireKind::U8, WireShape::One, 1, 1) && field(t, 20).enum_names != nullptr);
    CHECK(is(field(t, 22), WireKind::I64, WireShape::One, 8, 1));
    CHECK(field(t, 20).enum_names() == nullptr); // Mood has no NNH_DATA_ENUM
    const nnh::detail::EnumInfo* names = field(t, 21).enum_names();
    CHECK(names != nullptr && names->count == 3 && names->values[1] == 5);
    CHECK(field(t, 6).enum_names == nullptr); // not an enum
}

NNH_TEST(save_declaration_deduces_every_sequence_shape) {
    const SaveTypeInfo& t = info<Seqs>();
    CHECK(is(field(t, 1), WireKind::U16, WireShape::Array, 2, 3));
    CHECK(is(field(t, 2), WireKind::Bool, WireShape::List, 1, 4));
    CHECK((field(t, 2).items_offset == nnh::data::List<bool, 4>::items_offset()));
    CHECK(is(field(t, 3), WireKind::String, WireShape::List, 0, 3) && field(t, 3).capacity == 4);
    CHECK(field(t, 3).stride == sizeof(nnh::data::String<4>));
    CHECK(is(field(t, 4), WireKind::Struct, WireShape::List, 0, 4) && field(t, 4).nested == &info<Pair>());
    CHECK(is(field(t, 5), WireKind::Struct, WireShape::Array, 0, 2) && field(t, 5).stride == sizeof(Pair));
}

NNH_TEST(save_declaration_sorts_fields_by_id_whatever_the_order) {
    const SaveTypeInfo& t = info<Kinds>();
    CHECK(t.field_count == 22);
    for (int i = 0; i < t.field_count; ++i) CHECK(t.fields[i].id == i + 1);
    CHECK(field(t, 22).offset == offsetof(Kinds, big));
}

NNH_TEST(save_declaration_keeps_key_hash_version_and_role) {
    const SaveTypeInfo& k = info<Kinds>();
    CHECK(std::string_view(k.key) == "kinds" && k.key_hash == nnh::detail::fnv1a32("kinds"));
    CHECK(k.version == 1 && k.role == SaveRole::Type && k.step_count == 0 && k.size == sizeof(Kinds));
    const SaveTypeInfo& p = info<Pair>();
    CHECK(p.role == SaveRole::Struct && std::string_view(p.key).empty() && p.version == 0 && p.key_hash == 0);
    CHECK(p.depth == 1 && k.depth == 2);
}

NNH_TEST(save_declaration_lists_retired_ids_and_steps_in_order) {
    const SaveTypeInfo& v = info<Versioned>();
    CHECK(v.version == 3 && v.field_count == 2 && v.fields[0].id == 2 && v.fields[1].id == 7);
    CHECK(v.retired_count == 3 && v.retired[0] == 4 && v.retired[1] == 5 && v.retired[2] == 9);
    CHECK(v.step_count == 2 && v.steps[0].from == 1 && v.steps[1].from == 2);
    CHECK(v.steps[0].run != nullptr && v.steps[1].run != nullptr && v.steps[0].run != v.steps[1].run);
}

NNH_TEST(save_declaration_of_a_summary_reads_up_to_its_version) {
    const SaveTypeInfo& s = info<VersionedSummary>();
    CHECK(s.role == SaveRole::Summary && s.version == 3 && s.step_count == 0);
    CHECK(s.key_hash == info<Versioned>().key_hash);
}

NNH_TEST(save_declaration_works_in_the_global_namespace_and_beside_a_data_type) {
    CHECK(info<GlobalSave>().field_count == 1 && std::string_view(info<GlobalSave>().key) == "global");
    const SaveTypeInfo& item = info<test_data::Item>();
    CHECK(item.field_count == 3 && field(item, 1).kind == WireKind::String && field(item, 1).capacity == 16);
    CHECK(field(item, 3).kind == WireKind::TextureRef);
}

NNH_TEST(save_max_size_counts_every_string_and_list_full) {
    // GlobalSave: header 16 + "global" 6, then wire, id, 4 bytes
    static_assert(nnh::save::max_size<GlobalSave>() == 16 + 6 + 6);
    // Pair: fields (1 + 1 + 1) + (1 + 1 + 2); a row 1 + 2; a template: count 1 + 2 × (wire + id)
    CHECK(info<Pair>().max_fields == 7 && info<Pair>().max_row == 3 && info<Pair>().template_size == 5);
    // Seqs: numbers 2 + 1 + 6, flags 2 + 1 + 4, words 2 + 1 + 1 + 3 × 5, pairs 2 + 1 + 5 + 1 + 4 × 3,
    // grid 2 + 1 + 5 + 1 + 2 × 3
    static_assert(nnh::save::max_size<Seqs>() == 16 + 4 + 9 + 7 + 19 + 21 + 15);
    // Kinds: 19 tagged one-value fields of widths 1 1 1 2 2 4 4 8 8 4 8 8 4 4 4 4 4, name 1 + 8, pair 1 + 7,
    // three enums 1 + 1 + 8, each with wire and id (2 bytes)
    constexpr std::size_t values = 1 + 1 + 1 + 2 + 2 + 4 + 4 + 8 + 8 + 4 + 8 + 8 + 4 + 4 + 4 + 4 + 4 + 9 + 8 + 1 + 1 + 8;
    static_assert(nnh::save::max_size<Kinds>() == 16 + 5 + values + 22 * 2);
    static_assert(sizeof(nnh::save::Buffer<Kinds>::bytes) == nnh::save::max_size<Kinds>());
    static_assert(sizeof(nnh::save::Buffer<GlobalSave, 100>::bytes) == 128);
    static_assert(alignof(nnh::save::Buffer<Kinds>) >= 4);
}

NNH_TEST(save_declaration_keeps_names_only_outside_release) {
    const SaveTypeInfo& k = info<Kinds>();
    if (nnh::build::flavor() == nnh::build::Flavor::Release) {
        CHECK(std::string_view(field(k, 18).name).empty() && std::string_view(k.name).empty());
    } else {
        CHECK(std::string_view(field(k, 18).name) == "name" && std::string_view(k.name) == "Kinds");
    }
    CHECK(std::string_view(k.key) == "kinds"); // keys always stay
}
