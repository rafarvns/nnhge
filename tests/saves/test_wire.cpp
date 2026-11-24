// Wire primitives of structured saves (spec 027 T1.1): varints against hand-written bytes, wire bytes, widths.

#include <cstdint>
#include <string_view>

#include "saves/save_wire.hpp"
#include "test.hpp"

namespace {

using nnh::save::WireKind;
using nnh::save::WireShape;

bool writes(std::uint32_t value, std::initializer_list<std::uint8_t> expected) {
    std::uint8_t out[4] = {0xEE, 0xEE, 0xEE, 0xEE};
    const int n = nnh::detail::put_varint(out, value);
    if (n != static_cast<int>(expected.size()) || out[n] != 0xEE) return false;
    int i = 0;
    for (std::uint8_t b : expected) {
        if (out[i++] != b) return false;
    }
    return true;
}

int reads(std::initializer_list<std::uint8_t> bytes, std::uint32_t& value) {
    return nnh::detail::get_varint(bytes.begin(), bytes.end(), value);
}

} // namespace

NNH_TEST(save_varints_are_little_endian_base_128) {
    CHECK(writes(0, {0x00}));
    CHECK(writes(127, {0x7F}));
    CHECK(writes(128, {0x80, 0x01}));
    CHECK(writes(16383, {0xFF, 0x7F}));
    CHECK(writes(16384, {0x80, 0x80, 0x01}));
    CHECK(writes((1u << 21) - 1, {0xFF, 0xFF, 0x7F}));
}

NNH_TEST(save_varints_read_back_what_was_written) {
    std::uint32_t v = 99;
    CHECK(reads({0x00}, v) == 1 && v == 0);
    CHECK(reads({0x7F}, v) == 1 && v == 127);
    CHECK(reads({0x80, 0x01}, v) == 2 && v == 128);
    CHECK(reads({0xFF, 0x7F}, v) == 2 && v == 16383);
    CHECK(reads({0x80, 0x80, 0x01}, v) == 3 && v == 16384);
    CHECK(reads({0xFF, 0xFF, 0x7F, 0x55}, v) == 3 && v == (1u << 21) - 1);
    const std::uint8_t two[] = {0xAC, 0x02};
    const std::uint8_t* p = two;
    CHECK(nnh::detail::take_varint(p) == 300 && p == two + 2);
}

NNH_TEST(save_varints_refuse_overlong_cut_and_four_byte_forms) {
    std::uint32_t v = 7;
    CHECK(reads({0x80, 0x00}, v) == 0);             // 0 written in two bytes
    CHECK(reads({0xFF, 0x80, 0x00}, v) == 0);       // 127 written in three bytes
    CHECK(reads({0x80, 0x80, 0x80, 0x01}, v) == 0); // a fourth byte
    CHECK(reads({0x80}, v) == 0);                   // cut by the end
    CHECK(reads({0xFF, 0xFF}, v) == 0);
    CHECK(reads({}, v) == 0);
    CHECK(v == 7); // untouched on failure
}

NNH_TEST(save_varint_sizes_match_the_bytes_written) {
    CHECK(nnh::detail::varint_size(0) == 1 && nnh::detail::varint_size(127) == 1);
    CHECK(nnh::detail::varint_size(128) == 2 && nnh::detail::varint_size(16383) == 2);
    CHECK(nnh::detail::varint_size(16384) == 3 && nnh::detail::varint_size(65535) == 3);
}

NNH_TEST(save_wire_bytes_pack_kind_and_shape) {
    using nnh::detail::wire_byte;
    CHECK(wire_byte(WireKind::U16, WireShape::One) == 0x04);
    CHECK(wire_byte(WireKind::Bool, WireShape::Array) == 0x21);
    CHECK(wire_byte(WireKind::Struct, WireShape::List) == 0x53);
    CHECK(nnh::detail::wire_kind(0x53) == WireKind::Struct);
    CHECK(nnh::detail::wire_shape(0x53) == WireShape::List);
}

NNH_TEST(save_wire_bytes_are_valid_only_for_known_kinds_and_shapes) {
    int valid = 0;
    for (int b = 0; b < 256; ++b) {
        const bool ok = nnh::detail::wire_valid(static_cast<std::uint8_t>(b));
        const int kind = b & 0x1F;
        const int shape = (b >> 5) & 3;
        CHECK(ok == (b < 0x80 && kind >= 1 && kind <= 19 && shape <= 2));
        valid += ok ? 1 : 0;
    }
    CHECK(valid == 19 * 3);
}

NNH_TEST(save_wire_widths_follow_the_format) {
    using nnh::detail::wire_width;
    CHECK(wire_width(WireKind::Bool) == 1 && wire_width(WireKind::I8) == 1);
    CHECK(wire_width(WireKind::U16) == 2 && wire_width(WireKind::I16) == 2);
    CHECK(wire_width(WireKind::U32) == 4 && wire_width(WireKind::F32) == 4 && wire_width(WireKind::Color) == 4);
    CHECK(wire_width(WireKind::DataRef) == 4 && wire_width(WireKind::MusicRef) == 4);
    CHECK(wire_width(WireKind::U64) == 8 && wire_width(WireKind::F64) == 8 && wire_width(WireKind::Vec2) == 8);
    CHECK(wire_width(WireKind::String) == 0 && wire_width(WireKind::Struct) == 0);
}

NNH_TEST(save_field_paths_format_ids_and_items) {
    char text[64];
    const nnh::save::FieldPath p = nnh::save::path(9).item(12).field(4);
    CHECK(p.format(text, sizeof text) == 7);
    CHECK(std::string_view(text) == "9[12].4");
    CHECK(nnh::save::path(4, 2).format(text, sizeof text) == 3 && std::string_view(text) == "4.2");
    CHECK(nnh::save::FieldPath{}.format(text, sizeof text) == 0 && text[0] == '\0');
    CHECK(p.format(text, 4) == 3 && std::string_view(text) == "9[1"); // cut to the capacity
}

NNH_TEST(save_field_paths_refuse_more_than_sixteen_steps) {
    nnh::save::FieldPath p;
    for (int i = 0; i < 16; ++i) p = p.field(1);
    CHECK(p.valid() && p.size() == 16);
    CHECK(!p.item(0).valid());
    CHECK(!nnh::save::path(1).item(-1).valid());
}
