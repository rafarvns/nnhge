// Log matching for scripts (remote/log_match.hpp, spec 013 T3.2): patterns and the engine line prefix.

#include "remote/log_match.hpp"
#include "test.hpp"

using namespace nnh::tool::remote;

namespace {
const char* const kLine = "[f61       1.017s] I input  pressed A";
}

NNH_TEST(plain_text_matches_anywhere_in_the_line) {
    CHECK(log_matches("pressed A", kLine));
    CHECK(log_matches("[f61", kLine));
    CHECK(log_matches("", kLine));
    CHECK(!log_matches("pressed B", kLine));
    CHECK(!log_matches("pressed A!", kLine));
}

NNH_TEST(star_matches_any_run_of_characters) {
    CHECK(log_matches("input*A", kLine));
    CHECK(log_matches("pressed*", kLine));
    CHECK(log_matches("*", ""));
    CHECK(log_matches("I*input*pressed", kLine));
    CHECK(!log_matches("pressed*B", kLine));
    CHECK(log_matches("a*b*c", "xx a1b2b3c yy"));
    CHECK(!log_matches("a*b*c", "a b"));
}

NNH_TEST(question_mark_matches_exactly_one_character) {
    CHECK(log_matches("pressed ?", kLine));
    CHECK(log_matches("p?essed", kLine));
    CHECK(!log_matches("pressed A?", kLine));
    CHECK(!log_matches("?", ""));
}

NNH_TEST(escaped_wildcards_are_literal) {
    CHECK(log_matches("a\\*b", "x a*b y"));
    CHECK(!log_matches("a\\*b", "a12b"));
    CHECK(log_matches("what\\?", "say what?"));
    CHECK(!log_matches("what\\?", "say whatx"));
    CHECK(log_matches("c:\\\\x", "path c:\\x"));
    CHECK(log_matches("a\\b", "a\\b"));   // a backslash before another character is itself
    CHECK(log_matches("end\\", "the end\\"));
}

NNH_TEST(matching_is_case_sensitive) {
    CHECK(!log_matches("PRESSED", kLine));
}

NNH_TEST(engine_lines_give_their_frame_and_level) {
    const LineInfo a = line_info(kLine);
    CHECK(a.has_frame && a.frame == 61 && a.level == 'I');
    const LineInfo b = line_info("[f4000000000 0.000s] F crash  assert failed");
    CHECK(b.has_frame && b.frame == 4000000000u && b.level == 'F');
    const LineInfo c = line_info("[f0        0.000s] E fs     no");
    CHECK(c.has_frame && c.frame == 0 && c.level == 'E');
}

NNH_TEST(other_lines_have_no_frame) {
    for (const char* s : {"", "hello", "crash: data abort", "[f 0.0s] I app x", "[fx 0.0s] I app x",
                          "[f12 0.0s] Q app x", "[f99999999999 0.0s] I app x", "[f-1 0.0s] I app x"}) {
        const LineInfo info = line_info(s);
        CHECK(!info.has_frame && info.level == 0);
    }
}
