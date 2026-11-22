// The TOML-subset reader (spec 014 T4.1, FR-5, FR-14): lines, comments, keys, encodings and limits, each
// problem with its line.

#include <string>

#include "test.hpp"
#include "toml.hpp"

using namespace nnh::assetc::toml;

namespace {

bool parses(std::string_view text) {
    Document d;
    return d.parse(text);
}

/// "<line>: <message>" of a refused text ("ok" if accepted).
std::string problem_of(std::string_view text) {
    Document d;
    if (d.parse(text)) return "ok";
    return std::to_string(d.problem().line) + ": " + d.problem().message;
}

bool starts(const std::string& text, const char* prefix) {
    return text.rfind(prefix, 0) == 0;
}

} // namespace

NNH_TEST(toml_reads_keys_and_values_with_their_lines) {
    Document d;
    CHECK(d.parse("name = \"Goblin\"\n\nhp = 30\n"));
    CHECK(d.root().entries.size() == 2);
    const Entry* name = d.root().find("name");
    CHECK(name != nullptr && name->kind == EntryKind::Value && name->line == 1);
    CHECK(name->value.type == Type::String && name->value.text == "Goblin" && name->value.line == 1);
    const Entry* hp = d.root().find("hp");
    CHECK(hp != nullptr && hp->line == 3 && hp->value.integer == 30);
    CHECK(d.root().find("speed") == nullptr);
}

NNH_TEST(toml_keeps_keys_in_file_order) {
    Document d;
    CHECK(d.parse("b = 1\na = 2\nc = 3"));
    CHECK(d.root().entries[0].key == "b" && d.root().entries[1].key == "a" && d.root().entries[2].key == "c");
}

NNH_TEST(toml_accepts_comments_blank_lines_and_spaces) {
    Document d;
    CHECK(d.parse("# an enemy\n\n  \t\nhp=1 # its health\n\tspeed   =   2.5\t# fast\n# end"));
    CHECK(d.root().find("hp")->value.integer == 1 && d.root().find("speed")->value.number == 2.5f);
    CHECK(parses(""));
    CHECK(parses("# only a comment"));
    CHECK(parses("a = \"# not a comment\""));
}

NNH_TEST(toml_accepts_bare_keys_of_every_allowed_character) {
    Document d;
    CHECK(d.parse("hp_max = 1\nhurt-sound = 2\n2nd = 3\nABC = 4\n"));
    CHECK(d.root().find("hurt-sound") != nullptr && d.root().find("2nd") != nullptr);
}

NNH_TEST(toml_accepts_crlf_and_a_bom) {
    Document d;
    CHECK(d.parse("\xEF\xBB\xBFhp = 1\r\nspeed = 2\r\n"));
    CHECK(d.root().find("hp")->line == 1 && d.root().find("speed")->line == 2);
    CHECK(problem_of("hp = 1\rspeed = 2") == "1: a carriage return must be followed by a line feed");
    CHECK(problem_of("a = 1\n\xEF\xBB\xBF b = 2") ==
          "2: not valid UTF-8 (or a byte order mark not at the start)");
}

NNH_TEST(toml_refuses_text_that_isnt_utf8_or_has_control_characters) {
    CHECK(problem_of("a = 1\nname = \"\xC3\x28\"") ==
          "2: not valid UTF-8 (or a byte order mark not at the start)");
    CHECK(problem_of("a = 1\n\nb = \"x\x01y\"") == "3: control character 0x01");
    CHECK(problem_of("a = 1 # \x7F") == "1: control character 0x7F");
    CHECK(problem_of(std::string("a = 1\0", 6)) == "1: control character 0x00");
    CHECK(problem_of("a = 1\r") == "1: a carriage return must be followed by a line feed"); // at the very end
}

NNH_TEST(toml_refuses_files_and_lines_over_their_limits) {
    std::string fits;
    while (fits.size() + 100 <= kMaxFileBytes)
        fits += std::string(99, '#') + "\n";
    fits += std::string(kMaxFileBytes - fits.size(), '\n');
    CHECK(fits.size() == kMaxFileBytes && parses(fits));
    CHECK(problem_of(fits + "\n") == "1: the file is larger than 64 KiB");
    const std::string line(kMaxLineBytes, '#');
    CHECK(parses("a = 1\n" + line + "\r\n"));
    CHECK(problem_of("a = 1\n" + line + "#\n") == "2: the line is 1025 bytes: at most 1024");
}

NNH_TEST(toml_refuses_keys_it_doesnt_support) {
    CHECK(problem_of("stats.hp = 1") == "1: dotted keys aren't supported: write a [stats] section");
    CHECK(problem_of("\"hp\" = 1") == "1: quoted keys aren't supported");
    CHECK(problem_of("'hp' = 1") == "1: quoted keys aren't supported");
    CHECK(problem_of("hp 1") == "1: expected '=' after the key");
    CHECK(problem_of("hp") == "1: expected '=' after the key");
    CHECK(problem_of("= 1") == "1: expected a key, a [section] or a comment");
    CHECK(problem_of("h*p = 1") == "1: expected '=' after the key");
}

NNH_TEST(toml_refuses_a_key_set_twice) {
    CHECK(problem_of("hp = 1\nspeed = 2\nhp = 3") == "3: 'hp' is set twice (first on line 1)");
    CHECK(problem_of("[stats]\nhp = 1\nhp = 2") == "3: 'hp' is set twice (first on line 2)");
    CHECK(parses("hp = 1\n[stats]\nhp = 2")); // another table
}

NNH_TEST(toml_refuses_text_after_a_value) {
    CHECK(problem_of("hp = 1 2") == "1: expected a comment or the end of the line after the value");
    CHECK(problem_of("hp = 1\nname = \"a\" \"b\"") ==
          "2: expected a comment or the end of the line after the value");
    CHECK(problem_of("hp = 1, speed = 2") == "1: expected a comment or the end of the line after the value");
}

NNH_TEST(toml_reports_the_line_of_a_problem_after_a_multiline_array) {
    CHECK(problem_of("list = [\n  1,\n  2,\n]\nhp = 1\nhp = 2") == "6: 'hp' is set twice (first on line 5)");
    CHECK(starts(problem_of("list = [\n 1,\n x]"), "3: "));
}
