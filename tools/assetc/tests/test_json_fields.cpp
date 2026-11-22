// Typed field reads with readable refusals (json_fields.hpp).

#include <string>

#include "json.hpp"
#include "json_fields.hpp"
#include "test.hpp"

using namespace nnh::assetc;

namespace {

json::Document g_doc;

Fields fields(const char* text, const char* where = "layer \"a\"") {
    CHECK(g_doc.parse(text));
    return Fields(g_doc.root(), where);
}

} // namespace

NNH_TEST(fields_read_values_or_fallbacks) {
    Fields f = fields(R"({"w": 12, "o": 0.5, "v": false, "n": "x", "a": [1]})");
    CHECK(f.integer("w", 0, 1, 100) == 12);
    CHECK(f.integer("missing", 7, 1, 100) == 7);
    CHECK(f.number("o", 1, 0, 1) == 0.5);
    CHECK(f.number("missing", 1, 0, 1) == 1);
    CHECK(!f.boolean("v", true));
    CHECK(f.boolean("missing", true));
    CHECK(f.string("n") == "x");
    CHECK(f.string("missing", "d") == "d");
    CHECK(f.array("a").size() == 1);
    CHECK(!f.array("missing"));
    CHECK(f.has("w") && !f.has("missing"));
    CHECK(f.ok());
}

NNH_TEST(fields_refuse_values_out_of_range_with_the_key) {
    Fields f = fields(R"({"w": 0})");
    CHECK(f.integer("w", 1, 1, 1024) == 1);
    CHECK(!f.ok());
    CHECK(f.error() == "layer \"a\": \"w\" must be a whole number from 1 to 1024");
}

NNH_TEST(fields_refuse_fractions_and_other_types) {
    CHECK(![] { Fields f = fields(R"({"w": 1.5})"); f.integer("w", 0, 0, 9); return f.ok(); }());
    CHECK(![] { Fields f = fields(R"({"w": "1"})"); f.integer("w", 0, 0, 9); return f.ok(); }());
    CHECK(![] { Fields f = fields(R"({"o": 2})"); f.number("o", 0, 0, 1); return f.ok(); }());
    CHECK(![] { Fields f = fields(R"({"o": true})"); f.number("o", 0, 0, 1); return f.ok(); }());
    CHECK(![] { Fields f = fields(R"({"v": 1})"); f.boolean("v", false); return f.ok(); }());
    CHECK(![] { Fields f = fields(R"({"n": 1})"); f.string("n"); return f.ok(); }());
    CHECK(![] { Fields f = fields(R"({"a": {}})"); f.array("a"); return f.ok(); }());
}

NNH_TEST(fields_messages_name_the_expected_type) {
    Fields f = fields(R"({"o": 2})", "");
    f.number("o", 0, 0, 1);
    CHECK(f.error() == "\"o\" must be a number from 0 to 1");
    Fields g = fields(R"({"v": 1})", "map");
    g.boolean("v", false);
    CHECK(g.error() == "map: \"v\" must be true or false");
}

NNH_TEST(fields_need_required_members) {
    Fields f = fields(R"({})", "tileset");
    CHECK(f.required_integer("tilewidth", 1, 64) == 0);
    CHECK(f.error() == "tileset: \"tilewidth\" is missing");
    Fields g = fields(R"({})", "tileset");
    CHECK(g.required_string("image").empty());
    CHECK(g.error() == "tileset: \"image\" is missing");
}

NNH_TEST(fields_keep_the_first_problem) {
    Fields f = fields(R"({"w": -1, "o": 5})");
    f.integer("w", 0, 0, 9);
    f.number("o", 0, 0, 1);
    CHECK(f.error().find("\"w\"") != std::string::npos);
    CHECK(!f.refuse("later"));
    CHECK(f.error().find("later") == std::string::npos);
}
