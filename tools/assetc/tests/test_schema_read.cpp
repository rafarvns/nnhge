// Reading the schema description (spec 014 T5.1, FR-3): the tests' schema (tests/data_assets/test_types.cpp,
// compiled into this program too) written by core's writer and read back gives the same fingerprints and the
// same text; a damaged or inconsistent description is refused with its line.

#include <cstdio>
#include <string>

#include "data/schema_fingerprint.hpp"
#include "data/schema_write.hpp"
#include "schema_read.hpp"
#include "test.hpp"
#include "test_types.hpp"

using nnh::assetc::read_schema;
using nnh::assetc::Schema;
namespace detail = nnh::detail;

namespace {

void append(void* context, const char* text, std::size_t size) {
    static_cast<std::string*>(context)->append(text, size);
}

std::string describe(const detail::DataRegistry& r) {
    std::string out;
    detail::write_schema(r, append, &out);
    return out;
}

std::string tests_schema() {
    return describe(detail::data_registry());
}

std::string error_of(const std::string& text) {
    Schema s;
    std::string error;
    return read_schema(text, s, error) ? "ok" : error;
}

const char* kSmall = "nnh-data-schema 1\n"
                     "type Coin coin 1 %08X coins.cpp\n"
                     "field value one i32 min 1\n";

std::string small(const char* record = "") {
    // The real fingerprint of kSmall's type, computed from a read with a placeholder.
    Schema s;
    std::string error;
    char text[256];
    std::snprintf(text, sizeof text, kSmall, 0u);
    std::uint32_t fp = 0;
    if (!read_schema(text, s, error)) {
        const std::size_t at = error.find("computes ");
        if (at != std::string::npos)
            fp = static_cast<std::uint32_t>(std::stoul(error.substr(at + 9, 8), nullptr, 16));
    }
    std::snprintf(text, sizeof text, kSmall, static_cast<unsigned>(fp));
    return std::string(text) + record;
}

} // namespace

NNH_TEST(schema_read_gives_the_compiled_fingerprints) {
    Schema s;
    std::string error;
    CHECK(read_schema(tests_schema(), s, error));
    CHECK(error.empty());
    for (const detail::TypeNode* n = detail::data_registry().types; n != nullptr; n = n->next) {
        const detail::TypeInfo* read = s.type_by_name(n->info->name);
        CHECK(read != nullptr);
        if (read == nullptr) continue;
        const std::uint32_t compiled = detail::schema_fingerprint(detail::data_registry(), *n->info);
        CHECK(detail::schema_fingerprint(s.registry(), *read) == compiled);
        CHECK(s.fingerprint(*read) == compiled);
        CHECK(read->field_count == n->info->field_count);
    }
    CHECK(s.type_by_key("enemy") == s.type_by_name("Enemy"));
    CHECK(s.type_by_key("item") != nullptr && s.type_by_key("Drop") == nullptr &&
          s.type_by_key("") == nullptr);
}

NNH_TEST(schema_read_then_written_gives_the_same_text) {
    Schema s;
    std::string error;
    CHECK(read_schema(tests_schema(), s, error));
    CHECK(describe(s.registry()) == tests_schema());
}

NNH_TEST(schema_read_keeps_every_field_property) {
    Schema s;
    std::string error;
    CHECK(read_schema(tests_schema(), s, error));
    const detail::TypeInfo* enemy = s.type_by_key("enemy");
    const detail::FieldInfo& name = enemy->fields[0];
    CHECK(std::string(name.name) == "name" && name.kind == detail::FieldKind::String && name.capacity == 24 &&
          name.required);
    const detail::FieldInfo& level = enemy->fields[10];
    CHECK(level.kind == detail::FieldKind::Int && level.width == 1 && !level.is_signed && level.min == 1 &&
          level.max == 99);
    const detail::FieldInfo& drops = enemy->fields[6];
    CHECK(drops.shape == detail::FieldShape::List && drops.count == 4 &&
          drops.kind == detail::FieldKind::Struct);
    CHECK(detail::find_type(s.registry(), drops.target) == s.type_by_name("Drop"));
    const detail::FieldInfo& flags = enemy->fields[9];
    CHECK(flags.shape == detail::FieldShape::Array && flags.count == 3 &&
          flags.kind == detail::FieldKind::Bool);
    const detail::FieldInfo& speed = enemy->fields[2];
    CHECK(speed.min == 0 && speed.max == 3.4028234663852886e38);
    const detail::EnumInfo* element = detail::find_enum(s.registry(), enemy->fields[3].target);
    CHECK(element != nullptr && element->count == 3 && std::string(element->name) == "Element");
    CHECK(std::string(enemy->source).find("test_types.cpp") != std::string::npos);
    const detail::FieldInfo& price = s.type_by_key("item")->fields[1];
    CHECK(price.min == 0 && price.max == 2147483647.0 && price.is_signed && price.width == 4);
}

NNH_TEST(schema_read_accepts_a_small_description) {
    CHECK(error_of(small()) == "ok");
    CHECK(error_of("nnh-data-schema 1\n") == "ok");
}

NNH_TEST(schema_read_refuses_a_wrong_header_or_record) {
    CHECK(error_of("") == "line 1: expected 'nnh-data-schema 1'");
    CHECK(error_of("nnh-data-schema 2\n") == "line 1: expected 'nnh-data-schema 1'");
    CHECK(error_of(small("thing x\n")) == "line 4: unknown record 'thing'");
    CHECK(error_of("nnh-data-schema 1\nfield a one bool\n") == "line 2: a field outside a type");
    CHECK(error_of(small("field extra one bool\n")) == "line 4: a field outside a type");
    CHECK(error_of("nnh-data-schema 1\ntype A a 2 00000000 a.cpp\nfield x one bool\n") ==
          "line 4: type A: expected 2 fields, found 1");
}

NNH_TEST(schema_read_refuses_bad_fields) {
    const std::string head = "nnh-data-schema 1\ntype A a 1 00000000 a.cpp\n";
    CHECK(error_of(head + "field x one i64\n") == "line 3: unknown field kind 'i64'");
    CHECK(error_of(head + "field x some bool\n") == "line 3: unknown field shape 'some'");
    CHECK(error_of(head + "field x array 0 bool\n") == "line 3: bad count '0'");
    CHECK(error_of(head + "field x list 257 bool\n") == "line 3: bad count '257'");
    CHECK(error_of(head + "field x one string 256\n") == "line 3: bad capacity '256'");
    CHECK(error_of(head + "field x one struct Nope\n") == "line 3: unknown type 'Nope'");
    CHECK(error_of(head + "field x one enum Nope\n") == "line 3: unknown enum 'Nope'");
    CHECK(error_of(head + "field x one i32 min abc\n") == "line 3: bad number 'abc'");
    CHECK(error_of(head + "field " + std::string(64, 'f') + " one bool\n") ==
          "line 3: a name is longer than 63 characters");
    CHECK(error_of("nnh-data-schema 1\ntype A a 1 xyz a.cpp\nfield x one bool\n") ==
          "line 2: bad fingerprint 'xyz'");
    CHECK(error_of(head + "field x one bool min 1\n") == "line 3: min/max on a field that isn't a number");
    CHECK(error_of(head + "field x one bool what\n") == "line 3: unexpected 'what'");
    CHECK(error_of(head + "field x\n") == "line 3: a field needs a name, a shape and a kind");
}

NNH_TEST(schema_read_refuses_bad_names_and_counts) {
    const std::string long_name(64, 'n');
    CHECK(error_of("nnh-data-schema 1\nenum " + long_name + " 1 A\n") ==
          "line 2: a name is longer than 63 characters");
    CHECK(error_of("nnh-data-schema 1\nenum E 2 A\n") == "line 2: enum E: expected 2 names, found 1");
    CHECK(error_of("nnh-data-schema 1\nenum E 256 A\n") == "line 2: enum E: bad name count '256'");
    CHECK(error_of("nnh-data-schema 1\ntype A a 65 00000000 a.cpp\n") ==
          "line 2: type A: bad field count '65'");
    std::string many = "nnh-data-schema 1\n";
    for (int i = 0; i < 129; ++i)
        many += "type T" + std::to_string(i) + " - 0 00000000 a.cpp\n";
    CHECK(error_of(many) == "line 130: more than 128 types");
}

NNH_TEST(schema_read_refuses_a_fingerprint_that_doesnt_match) {
    const std::string error =
        error_of("nnh-data-schema 1\ntype Coin coin 1 12345678 coins.cpp\nfield value one i32 min 1\n");
    CHECK(error.rfind("schema: type Coin: the description says 12345678, nnh-assetc computes ", 0) == 0);
}

NNH_TEST(schema_read_checks_the_schema_like_the_host_program) {
    const std::string text = small() + "type Other coin 1 00000000 other.cpp\nfield v one bool\n";
    const std::string error = error_of(text);
    CHECK(error.find("both use the key 'coin'") != std::string::npos);
}

NNH_TEST(schema_read_refuses_a_description_over_its_limit) {
    std::string big = "nnh-data-schema 1\n";
    big += std::string(nnh::assetc::kMaxDescriptionBytes, '\n');
    CHECK(error_of(big) == "schema: the description is larger than 1 MiB");
}
