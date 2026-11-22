// Declaring a data type's fields (spec 014 FR-1, FR-2): NNH_DATA_FIELD deduces each field's kind, width,
// shape and capacity from the member's C++ type at compile time, records its offset, and turns the options
// into an effective range clamped to the member's type. Only the tables are tested here (no registration: see
// test_schema_registry.cpp).

#include <cfloat>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "nnh/data.hpp"
#include "nnh/gfx.hpp"
#include "test.hpp"

namespace {

using nnh::data::List;
using nnh::data::Ref;
using nnh::data::String;
using nnh::detail::FieldInfo;
using nnh::detail::FieldKind;
using nnh::detail::FieldShape;
using nnh::detail::type_id;

enum class Element : std::uint8_t { Normal, Fire, Ice };
enum Signed16 : std::int16_t { Low = -3, High = 3 };

struct Stats {
    std::uint16_t hp = 10;
    std::int8_t attack = 1;
};

struct Item {
    int price = 0;
};

struct Everything {
    bool flag = true;
    std::int8_t i8 = -1;
    std::uint8_t u8 = 2;
    std::int16_t i16 = -3;
    std::uint16_t u16 = 4;
    std::int32_t i32 = -5;
    std::uint32_t u32 = 6;
    float speed = 1.5f;
    String<24> name;
    Element element = Element::Fire;
    Signed16 level = High;
    nnh::Vec2 spawn{1, 2};
    nnh::Color tint{1, 2, 3, 4};
    Stats stats;
    int numbers[3] = {1, 2, 3};
    List<float, 4> chances;
    List<Stats, 2> stat_list;
    Stats stat_array[2];
    Ref<Item> drop;
    nnh::data::TextureRef sprite;
    nnh::data::SoundRef hurt;
    nnh::data::MusicRef theme;
    int not_listed = 99;
};

struct EverythingFields {
    using Self = Everything;
    static constexpr FieldInfo list[] = {
        NNH_DATA_FIELD(flag),
        NNH_DATA_FIELD(i8),
        NNH_DATA_FIELD(u8, .min = -5, .max = 300),
        NNH_DATA_FIELD(i16, .min = -100, .max = 100),
        NNH_DATA_FIELD(u16, .min = 1, .max = 9999, .required = true),
        NNH_DATA_FIELD(i32),
        NNH_DATA_FIELD(u32),
        NNH_DATA_FIELD(speed, .min = 0, .max = 2.5),
        NNH_DATA_FIELD(name, .required = true),
        NNH_DATA_FIELD(element),
        NNH_DATA_FIELD(level),
        NNH_DATA_FIELD(spawn),
        NNH_DATA_FIELD(tint),
        NNH_DATA_FIELD(stats),
        NNH_DATA_FIELD(numbers, .min = 0),
        NNH_DATA_FIELD(chances, .max = 1),
        NNH_DATA_FIELD(stat_list),
        NNH_DATA_FIELD(stat_array),
        NNH_DATA_FIELD(drop),
        NNH_DATA_FIELD(sprite),
        NNH_DATA_FIELD(hurt),
        NNH_DATA_FIELD(theme),
    };
};

const FieldInfo& field(int i) { return EverythingFields::list[i]; }

} // namespace

NNH_TEST(data_field_records_name_and_offset) {
    CHECK(std::strcmp(field(0).name, "flag") == 0);
    CHECK(field(0).offset == offsetof(Everything, flag));
    CHECK(std::strcmp(field(21).name, "theme") == 0);
    CHECK(field(21).offset == offsetof(Everything, theme));
    CHECK(field(13).offset == offsetof(Everything, stats));
}

NNH_TEST(data_field_deduces_bool_and_float) {
    CHECK(field(0).kind == FieldKind::Bool && field(0).width == 1);
    CHECK(field(7).kind == FieldKind::Float && field(7).width == 4);
}

NNH_TEST(data_field_deduces_every_integer_width_and_sign) {
    const struct {
        int index, width;
        bool is_signed;
    } expected[] = {{1, 1, true}, {2, 1, false}, {3, 2, true}, {4, 2, false}, {5, 4, true}, {6, 4, false}};
    for (const auto& e : expected) {
        CHECK(field(e.index).kind == FieldKind::Int);
        CHECK(field(e.index).width == e.width);
        CHECK(field(e.index).is_signed == e.is_signed);
    }
}

NNH_TEST(data_field_integer_range_is_the_type_range_by_default) {
    CHECK(field(1).min == -128 && field(1).max == 127);
    CHECK(field(5).min == INT32_MIN && field(5).max == INT32_MAX);
    CHECK(field(6).min == 0 && field(6).max == UINT32_MAX);
}

NNH_TEST(data_field_options_narrow_the_range_and_are_clamped_to_the_type) {
    CHECK(field(2).min == 0 && field(2).max == 255); // u8 with .min = -5, .max = 300
    CHECK(field(3).min == -100 && field(3).max == 100);
    CHECK(field(4).min == 1 && field(4).max == 9999 && field(4).required);
    CHECK(!field(3).required);
}

NNH_TEST(data_field_float_range_defaults_to_finite_floats) {
    CHECK(field(7).min == 0 && field(7).max == 2.5);
    CHECK(field(15).min == -static_cast<double>(FLT_MAX) && field(15).max == 1);
}

NNH_TEST(data_field_deduces_strings_with_their_capacity) {
    CHECK(field(8).kind == FieldKind::String);
    CHECK(field(8).capacity == 24);
    CHECK(field(8).required);
    CHECK(field(8).min == 0 && field(8).max == 0); // no range on non-numbers
}

NNH_TEST(data_field_deduces_enums_with_their_width_and_type) {
    CHECK(field(9).kind == FieldKind::Enum && field(9).width == 1 && !field(9).is_signed);
    CHECK(field(9).target == type_id<Element>());
    CHECK(field(10).kind == FieldKind::Enum && field(10).width == 2 && field(10).is_signed);
}

NNH_TEST(data_field_deduces_vec2_color_and_nested_structs) {
    CHECK(field(11).kind == FieldKind::Vec2);
    CHECK(field(12).kind == FieldKind::Color);
    CHECK(field(13).kind == FieldKind::Struct);
    CHECK(field(13).target == type_id<Stats>());
    CHECK(field(13).stride == sizeof(Stats));
}

NNH_TEST(data_field_single_values_have_shape_one) {
    CHECK(field(0).shape == FieldShape::One && field(0).count == 1);
    CHECK(field(13).shape == FieldShape::One && field(13).count == 1);
}

NNH_TEST(data_field_deduces_fixed_arrays) {
    CHECK(field(14).shape == FieldShape::Array);
    CHECK(field(14).count == 3);
    CHECK(field(14).kind == FieldKind::Int && field(14).width == sizeof(int));
    CHECK(field(14).stride == sizeof(int));
    CHECK(field(14).min == 0);
    CHECK(field(17).shape == FieldShape::Array && field(17).count == 2);
    CHECK(field(17).kind == FieldKind::Struct && field(17).target == type_id<Stats>());
}

NNH_TEST(data_field_deduces_lists_with_their_capacity_and_item_offset) {
    CHECK(field(15).shape == FieldShape::List);
    CHECK(field(15).count == 4);
    CHECK(field(15).kind == FieldKind::Float);
    CHECK(field(15).items_offset == (List<float, 4>::items_offset()));
    CHECK(field(16).shape == FieldShape::List && field(16).count == 2);
    CHECK(field(16).kind == FieldKind::Struct && field(16).stride == sizeof(Stats));
    CHECK(field(16).items_offset == (List<Stats, 2>::items_offset()));
}

NNH_TEST(data_field_deduces_references) {
    CHECK(field(18).kind == FieldKind::DataRef && field(18).target == type_id<Item>());
    CHECK(field(19).kind == FieldKind::TextureRef);
    CHECK(field(20).kind == FieldKind::SoundRef);
    CHECK(field(21).kind == FieldKind::MusicRef);
    CHECK(field(19).target == nullptr);
}

NNH_TEST(data_type_table_describes_the_struct) {
    static constexpr nnh::detail::TypeInfo info =
        nnh::detail::make_type<Everything>("Everything", "every", "here.cpp", EverythingFields::list);
    CHECK(info.tag == type_id<Everything>());
    CHECK(std::strcmp(info.name, "Everything") == 0);
    CHECK(std::strcmp(info.key, "every") == 0);
    CHECK(std::strcmp(info.source, "here.cpp") == 0);
    CHECK(info.field_count == 22);
    CHECK(info.fields == EverythingFields::list);
    CHECK(info.size == sizeof(Everything));
}

NNH_TEST(data_type_reset_builds_the_struct_defaults_in_place) {
    static constexpr nnh::detail::TypeInfo info =
        nnh::detail::make_type<Everything>("Everything", "every", "here.cpp", EverythingFields::list);
    alignas(Everything) unsigned char storage[sizeof(Everything)];
    std::memset(storage, 0xAB, sizeof storage);
    info.reset(storage);
    const Everything& e = *reinterpret_cast<const Everything*>(storage);
    CHECK(e.flag && e.u16 == 4 && e.speed == 1.5f && e.element == Element::Fire);
    CHECK(e.name.empty() && e.chances.empty() && e.not_listed == 99);
    CHECK(e.stat_list.begin()[1].hp == 10);
}
