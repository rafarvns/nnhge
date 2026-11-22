// TOML values (spec 014 T4.2, FR-5, FR-16): booleans, integers, floats (parsed once with from_chars,
// correctly rounded), basic strings and their escapes, inline arrays; each refused TOML feature with its own
// message.

#include <cstring>
#include <limits>
#include <string>

#include "test.hpp"
#include "toml.hpp"

using namespace nnh::assetc::toml;

namespace {

/// The value of `v = <text>` (a Bool false on failure, which the tests' other checks catch).
Value value_of(const std::string& text) {
    Document d;
    if (!d.parse("v = " + text) || d.root().find("v") == nullptr) return {};
    return d.root().find("v")->value;
}

std::string problem_of(const std::string& value) {
    Document d;
    if (d.parse("v = " + value)) return "ok";
    return d.problem().message;
}

std::uint32_t bits(float f) {
    std::uint32_t b;
    std::memcpy(&b, &f, 4);
    return b;
}

} // namespace

NNH_TEST(toml_reads_booleans) {
    CHECK(value_of("true").type == Type::Bool && value_of("true").boolean);
    CHECK(value_of("false").type == Type::Bool && !value_of("false").boolean);
    CHECK(problem_of("True") == "text without quotes: strings are written \"True\"");
    CHECK(problem_of("truex") == "text without quotes: strings are written \"truex\"");
}

NNH_TEST(toml_reads_decimal_integers) {
    CHECK(value_of("0").type == Type::Integer && value_of("0").integer == 0);
    CHECK(value_of("-12").integer == -12 && value_of("+7").integer == 7);
    CHECK(value_of("1_000_000").integer == 1000000);
    CHECK(value_of("9223372036854775807").integer == std::numeric_limits<std::int64_t>::max());
    CHECK(value_of("-9223372036854775808").integer == std::numeric_limits<std::int64_t>::min());
    CHECK(value_of("9223372036854775808").out_of_range);
    CHECK(value_of("1_000").text == "1_000");
}

NNH_TEST(toml_refuses_malformed_integers) {
    CHECK(problem_of("012") == "leading zeros aren't allowed");
    CHECK(problem_of("-01") == "leading zeros aren't allowed");
    CHECK(problem_of("1__0") == "'_' must be between digits");
    CHECK(problem_of("_1") == "text without quotes: strings are written \"_1\"");
    CHECK(problem_of("1_") == "'_' must be between digits");
    CHECK(problem_of("--1") == "not a number: --1");
    CHECK(problem_of("+") == "not a number: +");
    CHECK(problem_of("1x") == "not a number: 1x");
}

NNH_TEST(toml_refuses_hexadecimal_octal_and_binary_numbers) {
    const char* kMessage = "hexadecimal, octal and binary numbers aren't supported";
    CHECK(problem_of("0xFF") == kMessage && problem_of("0o17") == kMessage &&
          problem_of("0b101") == kMessage);
}

NNH_TEST(toml_reads_floats_of_every_form) {
    CHECK(value_of("1.5").type == Type::Float && value_of("1.5").number == 1.5f);
    CHECK(value_of("-0.25").number == -0.25f && value_of("+3.0").number == 3.0f);
    CHECK(value_of("1e3").number == 1000.0f && value_of("1E3").number == 1000.0f);
    CHECK(value_of("2.5e-2").number == 0.025f && value_of("6.25E+1").number == 62.5f);
    CHECK(value_of("1_000.000_1").number == 1000.0001f);
    CHECK(value_of("0.0").number == 0.0f && value_of("-0.0").number == 0.0f);
    CHECK(value_of("1e05").number == 100000.0f); // zeros may lead an exponent
    CHECK(value_of("3.14").text == "3.14");
}

NNH_TEST(toml_rounds_hard_decimal_floats_correctly) {
    // Halfway and near-halfway cases between two floats: a naive parser (double first, then float, or digit
    // by digit) rounds some of them the wrong way.
    CHECK(bits(value_of("16777217.0").number) == 0x4B800000u); // 2^24 + 1 -> ties to even: 2^24
    CHECK(bits(value_of("16777219.0").number) == 0x4B800002u); // ties to even upwards
    CHECK(bits(value_of("1.00000005960464477539062500001").number) == 0x3F800001u); // just above halfway
    CHECK(bits(value_of("1.000000059604644775390625").number) == 0x3F800000u);      // exactly halfway: even
    CHECK(bits(value_of("0.1").number) == 0x3DCCCCCDu);
    CHECK(bits(value_of("3.4028234663852886e38").number) == 0x7F7FFFFFu); // FLT_MAX
}

NNH_TEST(toml_marks_floats_outside_floats_range) {
    CHECK(value_of("3.5e38").out_of_range);
    CHECK(value_of("-1e39").out_of_range);
    CHECK(!value_of("1e38").out_of_range);
}

NNH_TEST(toml_refuses_malformed_floats) {
    CHECK(problem_of("1.") == "not a number: 1.");
    CHECK(problem_of(".5") == "not a number: .5");
    CHECK(problem_of("1.e3") == "not a number: 1.e3");
    CHECK(problem_of("1e") == "not a number: 1e");
    CHECK(problem_of("01.5") == "leading zeros aren't allowed");
    CHECK(problem_of("1._5") == "'_' must be between digits");
    CHECK(problem_of("inf") == "inf and nan aren't supported");
    CHECK(problem_of("-inf") == "inf and nan aren't supported");
    CHECK(problem_of("+nan") == "inf and nan aren't supported");
}

NNH_TEST(toml_refuses_dates_and_times) {
    const char* kMessage = "dates and times aren't supported";
    CHECK(problem_of("1979-05-27") == kMessage);
    CHECK(problem_of("07:32:00") == kMessage);
    CHECK(problem_of("1979-05-27T07:32:00Z") == kMessage);
}

NNH_TEST(toml_reads_basic_strings_and_their_escapes) {
    CHECK(value_of("\"\"").type == Type::String && value_of("\"\"").text.empty());
    CHECK(value_of("\"a \\\"b\\\" \\\\ c\"").text == "a \"b\" \\ c");
    CHECK(value_of("\"line\\nnext\\tend\"").text == "line\nnext\tend");
    CHECK(value_of("\"\\u00E7\\u20AC\"").text == "\xC3\xA7\xE2\x82\xAC");
    CHECK(value_of("\"caf\xC3\xA9 \xF0\x9F\x98\x80\"").text == "caf\xC3\xA9 \xF0\x9F\x98\x80");
    CHECK(value_of("\"tab\there\"").text == "tab\there");
}

NNH_TEST(toml_refuses_strings_it_doesnt_support) {
    CHECK(problem_of("'literal'") == "literal strings ('...') aren't supported: use \"...\"");
    CHECK(problem_of("\"\"\"multi\"\"\"") == "multi-line strings aren't supported");
    CHECK(problem_of("\"open") == "unterminated string");
    CHECK(problem_of("\"bad \\x escape\"") ==
          "escape \\x isn't supported (use \\\" \\\\ \\n \\t or \\uXXXX)");
    CHECK(problem_of("\"\\r\"") == "escape \\r isn't supported (use \\\" \\\\ \\n \\t or \\uXXXX)");
    CHECK(problem_of("\"\\U0001F600\"") == "escape \\U isn't supported (use \\\" \\\\ \\n \\t or \\uXXXX)");
    CHECK(problem_of("\"\\u12\"") == "\\u needs 4 hexadecimal digits");
    CHECK(problem_of("\"\\uD800\"") == "\\uD800 isn't a character");
    CHECK(problem_of("\"\\u0000\"") == "\\u0000 isn't allowed");
}

NNH_TEST(toml_reads_inline_arrays) {
    const Value a = value_of("[1, 2.5, \"x\", true]");
    CHECK(a.type == Type::Array && a.items.size() == 4);
    CHECK(a.items[0].integer == 1 && a.items[1].number == 2.5f && a.items[2].text == "x" &&
          a.items[3].boolean);
    CHECK(value_of("[]").type == Type::Array && value_of("[]").items.empty());
    CHECK(value_of("[ 1 , 2 , ]").items.size() == 2);
    const Value nested = value_of("[[1, 2], [3, 4]]");
    CHECK(nested.items.size() == 2 && nested.items[1].items[0].integer == 3);
}

NNH_TEST(toml_reads_arrays_over_several_lines_with_comments) {
    Document d;
    CHECK(d.parse("v = [ # positions\n  [1, 2],   # first\n\n  [3, 4],\n] # done\nw = 1\n"));
    const Value& v = d.root().find("v")->value;
    CHECK(v.items.size() == 2 && v.items[0].line == 2 && v.items[1].line == 4);
    CHECK(d.root().find("w")->line == 6);
}

NNH_TEST(toml_refuses_malformed_arrays) {
    CHECK(problem_of("[1, 2") == "unterminated array: expected ']'");
    CHECK(problem_of("[1 2]") == "expected ',' or ']' in the array");
    CHECK(problem_of("[,]") == "expected a value");
    CHECK(problem_of("[1,,2]") == "expected a value");
    CHECK(problem_of("[[[1]]]") == "arrays nested more than 2 deep");
}

NNH_TEST(toml_refuses_inline_tables_and_missing_values) {
    CHECK(problem_of("{ hp = 1 }") == "inline tables ({...}) aren't supported: write a [section]");
    CHECK(problem_of("") == "expected a value");
    CHECK(problem_of("# nothing") == "expected a value");
    CHECK(problem_of("Fire") == "text without quotes: strings are written \"Fire\"");
}

NNH_TEST(toml_encodes_escapes_at_utf8_length_boundaries) {
    CHECK(value_of("\"\\u007E\\u0080\\u07FF\\u0800\"").text == "\x7E\xC2\x80\xDF\xBF\xE0\xA0\x80");
}
