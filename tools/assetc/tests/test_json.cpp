// The bounded JSON reader (spec 012 T5.1): grammar, values, escapes, UTF-8, limits and error positions.

#include <cmath>
#include <string>

#include "json.hpp"
#include "test.hpp"

using namespace nnh::assetc::json;

namespace {

bool parses(std::string_view text) {
    Document d;
    return d.parse(text);
}

std::string error_of(std::string_view text) {
    Document d;
    d.parse(text);
    return d.error();
}

bool mentions(const std::string& text, const char* part) { return text.find(part) != std::string::npos; }

} // namespace

NNH_TEST(json_reads_scalars) {
    Document d;
    CHECK(d.parse("true") && d.root().is_bool() && d.root().boolean());
    CHECK(d.parse("false") && d.root().is_bool() && !d.root().boolean());
    CHECK(d.parse("null") && d.root().is_null());
    CHECK(d.parse(" 42 ") && d.root().is_number() && d.root().number() == 42);
    CHECK(d.parse("\"hi\"") && d.root().is_string() && d.root().string() == "hi");
}

NNH_TEST(json_reads_numbers_of_every_form) {
    Document d;
    CHECK(d.parse("-0") && d.root().number() == 0);
    CHECK(d.parse("-12.5") && d.root().number() == -12.5);
    CHECK(d.parse("1e3") && d.root().number() == 1000);
    CHECK(d.parse("2.5E-1") && d.root().number() == 0.25);
    CHECK(d.parse("4294967295") && d.root().number() == 4294967295.0);
    CHECK(d.parse("0.1") && d.root().number() == 0.1);
}

NNH_TEST(json_refuses_numbers_outside_the_grammar) {
    for (const char* bad : {"01", "+1", "1.", ".5", "1e", "1e+", "-", "0x10", "NaN", "Infinity", "-Infinity", "1.e3"}) {
        CHECK(!parses(bad));
    }
}

NNH_TEST(json_refuses_numbers_too_long_or_out_of_range) {
    CHECK(!parses("1e999"));
    CHECK(mentions(error_of("1e999"), "out of range"));
    CHECK(parses(std::string(64, '1')));
    CHECK(!parses(std::string(65, '1')));
    CHECK(mentions(error_of(std::string(65, '1')), "number too long"));
}

NNH_TEST(json_reads_arrays_in_order) {
    Document d;
    CHECK(d.parse("[1, [2], {}, \"x\"]"));
    const Ref a = d.root();
    CHECK(a.is_array() && a.size() == 4);
    Ref v = a.first();
    CHECK(v.number() == 1);
    v = v.next();
    CHECK(v.is_array() && v.size() == 1 && v.first().number() == 2);
    v = v.next();
    CHECK(v.is_object() && v.size() == 0 && !v.first());
    v = v.next();
    CHECK(v.string() == "x");
    CHECK(!v.next());
}

NNH_TEST(json_reads_objects_by_key) {
    Document d;
    CHECK(d.parse("{\"a\": 1, \"b\": {\"c\": [true]}, \"a\": 2}"));
    const Ref o = d.root();
    CHECK(o.is_object() && o.size() == 3);
    CHECK(o.get("a").number() == 1); // the first of duplicated keys
    CHECK(o.get("b").get("c").first().boolean());
    CHECK(o.first().key() == "a");
    CHECK(o.first().next().key() == "b");
    CHECK(!o.get("zz"));
}

NNH_TEST(json_missing_values_answer_as_absent) {
    Document d;
    CHECK(d.parse("{\"a\": 1}"));
    const Ref missing = d.root().get("x").get("y");
    CHECK(!missing);
    CHECK(!missing.is_null() && !missing.is_number() && !missing.is_object());
    CHECK(missing.size() == 0 && missing.string().empty() && missing.number() == 0 && !missing.boolean());
    CHECK(!missing.first() && !missing.next() && missing.key().empty());
    CHECK(d.root().get("a").get("b").size() == 0); // get on a number: missing
}

NNH_TEST(json_decodes_escapes) {
    Document d;
    CHECK(d.parse(R"("a\"b\\c\/d\b\f\n\r\t")"));
    CHECK(d.root().string() == "a\"b\\c/d\b\f\n\r\t");
    CHECK(d.parse("\"A\\u00e7\\u20AC\""));
    CHECK(d.root().string() == "A\xC3\xA7\xE2\x82\xAC");
    CHECK(d.parse("\"\\uD83D\\uDE00\"")); // U+1F600 from a surrogate pair
    CHECK(d.root().string() == "\xF0\x9F\x98\x80");
    CHECK(d.parse("\"\\u07FF\\u0800\"")); // the last 2-byte and the first 3-byte code points
    CHECK(d.root().string() == "\xDF\xBF\xE0\xA0\x80");
    CHECK(d.parse("\"a\\u0000b\""));
    CHECK(d.root().string() == std::string_view("a\0b", 3));
}

NNH_TEST(json_refuses_bad_escapes_and_lone_surrogates) {
    for (const char* bad : {R"("\x")", R"("\u12")", R"("\u12G4")", R"("\uD83D")", R"("\uDE00")",
                            R"("\uD83DA")", R"("\uD83Dx")", R"("\uD83DA")", R"("\uD83D")", "\"abc",
                            "\"a\\"}) {
        CHECK(!parses(bad));
    }
    // A high surrogate followed by an escape that is not a low surrogate (written with "\\" + "u" on purpose).
    const std::string u = std::string("\\") + "u";
    for (const char* low : {"0041", "D83D", "E000", "DBFF"}) {
        CHECK(!parses("\"" + u + "D83D" + u + low + "\""));
    }
    CHECK(parses("\"" + u + "DBFF" + u + "DFFF\"")); // U+10FFFF, the last pair
}

NNH_TEST(json_refuses_control_characters_in_strings) {
    CHECK(!parses("\"a\nb\""));
    CHECK(!parses("\"a\x1F" "b\""));      // the last control character
    CHECK(parses("\"a\x7F" "b\""));       // DEL is allowed by JSON
    CHECK(!parses(std::string("\"a\0b\"", 5)));
    CHECK(mentions(error_of("\"a\tb\""), "control character"));
}

NNH_TEST(json_accepts_valid_utf8_and_refuses_invalid) {
    CHECK(parses("\"\xC3\xA7\xE2\x82\xAC\xF0\x9F\x98\x80\""));
    CHECK(parses("\"\xF4\x8F\xBF\xBF\"")); // U+10FFFF
    for (const char* bad : {"\"\xC3\"", "\"\x80\"", "\"\xC0\xAF\"", "\"\xE0\x80\xAF\"", "\"\xED\xA0\x80\"",
                            "\"\xF4\x90\x80\x80\"", "\"\xF8\x88\x80\x80\x80\"", "\"\xE2\x82\""}) {
        CHECK(!parses(bad));
        CHECK(mentions(error_of(bad), "UTF-8"));
    }
}

NNH_TEST(json_refuses_broken_structure) {
    for (const char* bad : {"", " ", "[", "]", "[1,]", "[1 2]", "{", "{\"a\"}", "{\"a\":}", "{\"a\":1,}", "{a:1}",
                            "{\"a\" 1}", "{1:2}", "[1] 2", "tru", "nul", "falsey", "// c\n1", "\xEF\xBB\xBF" "1"}) {
        CHECK(!parses(bad));
    }
}

NNH_TEST(json_error_names_line_and_column) {
    const std::string e = error_of("{\n  \"a\": 1,\n  \"b\" 2\n}");
    CHECK(mentions(e, "line 3, column 7"));
    CHECK(mentions(e, "expected ':'"));
    CHECK(mentions(error_of("[1] x"), "line 1, column 5"));
}

NNH_TEST(json_limits_depth_to_32) {
    CHECK(parses(std::string(32, '[') + std::string(32, ']')));
    CHECK(!parses(std::string(33, '[') + std::string(33, ']')));
    CHECK(mentions(error_of(std::string(33, '[') + std::string(33, ']')), "nested deeper than 32"));
    CHECK(!parses(std::string(100000, '['))); // no stack overflow
}

NNH_TEST(json_limits_the_document_size) {
    std::string big(kMaxDocument + 1, ' ');
    big[0] = '1';
    CHECK(!parses(big));
    CHECK(mentions(error_of(big), "larger than 64 MiB"));
}

NNH_TEST(json_limits_the_number_of_values) {
    Document d;
    const Limits ten{.max_values = 10};
    CHECK(d.parse("[0,1,2,3,4,5,6,7,8]", ten)); // 10 values with the array
    CHECK(!d.parse("[0,1,2,3,4,5,6,7,8,9]", ten));
    CHECK(mentions(d.error(), "too many values"));
    CHECK(Limits{}.max_values == kMaxValues && Limits{}.max_document == kMaxDocument);
}

NNH_TEST(json_a_new_parse_replaces_the_document) {
    Document d;
    CHECK(d.parse("{\"a\": \"long string here\"}"));
    CHECK(!d.parse("[")); // a failed parse leaves no document
    CHECK(!d.root());
    CHECK(d.parse("[7]"));
    CHECK(d.root().first().number() == 7 && d.error().empty());
}
