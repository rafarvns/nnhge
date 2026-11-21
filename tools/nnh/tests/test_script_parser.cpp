// Script parser (remote/script.hpp, spec 013 T3.3): every command and button, case, comments, quotes and
// escapes, every error as "file:line: what", limits, after `end`, and the warning for input after `expect`.

#include <string>
#include <vector>

#include "remote/script.hpp"
#include "test.hpp"

using namespace nnh::tool::remote;

namespace {

ParsedScript parse(const std::string& text) { return parse_script(text, "t.nnhs"); }

/// The single step of a one-command script (End excluded), or a default Step if it didn't parse.
Step one(const std::string& line) {
    const ParsedScript p = parse(line);
    if (!p.errors.empty() || p.script.steps.size() != 2) {
        Step bad;
        bad.line = -1;
        return bad;
    }
    return p.script.steps[0];
}

/// The first error of a script, or "" if it parsed.
std::string error_of(const std::string& text) {
    const ParsedScript p = parse(text);
    return p.errors.empty() ? std::string() : p.errors[0];
}

bool has(const std::string& s, const char* part) { return s.find(part) != std::string::npos; }

constexpr std::uint32_t kA = 1u << 0, kB = 1u << 1, kL = 1u << 4, kR = 1u << 5;

} // namespace

NNH_TEST(parser_reads_wait_press_release_hold_and_tap) {
    const Step wait = one("wait 30");
    CHECK(wait.command == Command::Wait && wait.frames == 30 && wait.line == 1);
    const Step press = one("press A");
    CHECK(press.command == Command::Press && press.buttons == kA);
    const Step release = one("release L+R");
    CHECK(release.command == Command::Release && release.buttons == (kL | kR));
    CHECK(one("release all").command == Command::ReleaseAll);
    const Step hold = one("hold L+R 20");
    CHECK(hold.command == Command::Hold && hold.buttons == (kL | kR) && hold.frames == 20);
    const Step tap = one("tap B");
    CHECK(tap.command == Command::Tap && tap.buttons == kB);
}

NNH_TEST(parser_knows_every_button) {
    const char* names[] = {"A", "B", "X", "Y", "L", "R", "ZL", "ZR", "START", "SELECT", "UP", "DOWN", "LEFT", "RIGHT"};
    for (std::uint32_t bit = 0; bit < 14; ++bit) {
        CHECK(one(std::string("press ") + names[bit]).buttons == (1u << bit));
    }
    CHECK(one("press A+B+A").buttons == (kA | kB));
}

NNH_TEST(parser_ignores_case_of_commands_and_names) {
    CHECK(one("PRESS select").buttons == (1u << 9));
    CHECK(one("Tap zl+Zr").buttons == ((1u << 6) | (1u << 7)));
    CHECK(one("RELEASE ALL").command == Command::ReleaseAll);
    CHECK(one("Stick CSTICK 0 1").stick == 1);
}

NNH_TEST(parser_reads_sticks_in_thousandths) {
    const Step s = one("stick circle 1 -0.5");
    CHECK(s.command == Command::Stick && s.stick == 0 && s.x == 1000 && s.y == -500);
    const Step c = one("stick cstick -1 0.125");
    CHECK(c.stick == 1 && c.x == -1000 && c.y == 125);
    const Step zero = one("stick circle 0 .5");
    CHECK(zero.x == 0 && zero.y == 500);
    CHECK(one("stick circle 1.000 -1.0").x == 1000);
}

NNH_TEST(parser_reads_touch_drag_and_untouch) {
    const Step t = one("touch 319 239");
    CHECK(t.command == Command::Touch && t.x == 319 && t.y == 239);
    const ParsedScript p = parse("touch 0 0\ndrag 280 200 30\nuntouch");
    CHECK(p.errors.empty() && p.script.steps.size() == 4);
    if (p.script.steps.size() != 4) return;
    const Step& d = p.script.steps[1];
    CHECK(d.command == Command::Drag && d.x == 280 && d.y == 200 && d.frames == 30 && d.line == 2);
    CHECK(p.script.steps[2].command == Command::Untouch && p.script.steps[2].line == 3);
}

NNH_TEST(parser_reads_screenshot_expect_fail_on_and_end) {
    const Step s = one("screenshot stick-right_2");
    CHECK(s.command == Command::Screenshot && s.text == "stick-right_2");
    const Step e = one("expect \"pressed A\" 10");
    CHECK(e.command == Command::Expect && e.text == "pressed A" && e.frames == 10);
    const Step f = one("fail-on \"] E \"");
    CHECK(f.command == Command::FailOn && f.text == "] E ");
    const ParsedScript p = parse("wait 1\nend\n");
    CHECK(p.errors.empty() && p.script.steps.size() == 2 && p.script.steps[1].command == Command::End);
    CHECK(p.script.steps.size() == 2 && p.script.steps[1].line == 2);
}

NNH_TEST(parser_unescapes_quotes_and_backslashes_and_keeps_pattern_escapes) {
    CHECK(one("expect \"say \\\"hi\\\"\" 5").text == "say \"hi\"");
    CHECK(one("expect \"a\\\\b\" 5").text == "a\\b");
    CHECK(one("expect \"a\\*b \\?\" 5").text == "a\\*b \\?");
    CHECK(one("fail-on \"# not a comment\"").text == "# not a comment");
    CHECK(one("fail-on \"x\"# a comment right after the quote").text == "x");
}

NNH_TEST(parser_skips_comments_and_blank_lines) {
    const ParsedScript p = parse("# title\n\n   \nwait 5   # five\n\t# tabbed\r\ntap A\r\n");
    CHECK(p.errors.empty() && p.script.steps.size() == 3 && p.script.lines == 6);
    CHECK(p.script.steps.size() == 3 && p.script.steps[0].line == 4 && p.script.steps[1].line == 6);
}

NNH_TEST(parser_adds_end_when_the_file_has_none) {
    const ParsedScript p = parse("wait 5\ntap A\n");
    CHECK(p.errors.empty() && p.script.steps.size() == 3);
    CHECK(p.script.steps.back().command == Command::End && p.script.steps.back().line == 2);
    const ParsedScript empty = parse("");
    CHECK(empty.errors.empty() && empty.script.steps.size() == 1 && empty.script.steps[0].line == 1);
}

NNH_TEST(parser_reports_unknown_commands_and_buttons_with_file_and_line) {
    CHECK(error_of("wait 1\njump 3") == "t.nnhs:2: unknown command 'jump'");
    CHECK(has(error_of("press Q"), "t.nnhs:1: unknown button 'Q'"));
    CHECK(has(error_of("press A+"), "t.nnhs:1: unknown button ''"));
    CHECK(has(error_of("tap A++B"), "unknown button"));
    CHECK(has(error_of("stick left 0 0"), "t.nnhs:1: stick is 'circle' or 'cstick'"));
}

NNH_TEST(parser_reports_wrong_argument_counts_with_the_usage) {
    for (const char* bad : {"wait", "wait 1 2", "press", "release", "hold A", "tap", "tap A 3", "stick circle 1",
                            "touch 1", "drag 1 2", "untouch 1", "screenshot", "expect \"x\"", "fail-on",
                            "end now", "release all A"}) {
        const std::string e = error_of(bad);
        CHECK(has(e, "t.nnhs:1: wrong number of arguments; usage: "));
    }
    CHECK(error_of("hold A") == "t.nnhs:1: wrong number of arguments; usage: hold B[+B...] N");
}

NNH_TEST(parser_reports_numbers_out_of_range) {
    for (const char* bad : {"wait 0", "wait 216001", "wait -1", "wait 1.5", "wait x", "hold A 0", "drag 1 1 0",
                            "expect \"x\" 216001", "wait 99999999999999999999"}) {
        CHECK(has(error_of(std::string("touch 0 0\n") + bad), "t.nnhs:2: frames must be a whole number 1 to 216000"));
    }
    CHECK(one("wait 216000").frames == 216000);
    for (const char* bad : {"stick circle 1.001 0", "stick circle 0 -2", "stick circle 0.0001 0", "stick circle x 0",
                            "stick circle - 0", "stick circle 1. 0", "stick circle 0 --1", "stick circle . 0"}) {
        CHECK(has(error_of(bad), "t.nnhs:1: stick values are -1 to 1"));
    }
    for (const char* bad : {"touch 320 0", "touch 0 240", "touch -1 0", "touch 1.5 0", "drag 0 240 5"}) {
        CHECK(has(error_of(std::string("touch 0 0\n") + bad), "t.nnhs:2: touch is X 0 to 319, Y 0 to 239"));
    }
}

NNH_TEST(parser_reports_drag_without_a_touch) {
    CHECK(error_of("drag 10 10 5") == "t.nnhs:1: drag needs a touch down first (touch X Y)");
    CHECK(has(error_of("touch 1 1\nuntouch\ndrag 10 10 5"), "t.nnhs:3: drag needs"));
    CHECK(has(error_of("touch 1 1\nrelease all\ndrag 10 10 5"), "t.nnhs:3: drag needs"));
    CHECK(error_of("touch 1 1\ndrag 2 2 2\ndrag 3 3 3").empty());
}

NNH_TEST(parser_reports_bad_names_and_texts) {
    for (const char* bad : {"screenshot a.b", "screenshot a/b", "screenshot 123456789012345678901234567890123"}) {
        CHECK(has(error_of(bad), "t.nnhs:1: NAME is 1 to 32 of A-Z a-z 0-9 _ -"));
    }
    CHECK(one("screenshot 12345678901234567890123456789012").text.size() == 32);
    CHECK(error_of("expect \"abc 5") == "t.nnhs:1: unterminated quote");
    CHECK(error_of("expect abc 5") == "t.nnhs:1: TEXT must be in double quotes");
    CHECK(error_of("fail-on \"\"") == "t.nnhs:1: TEXT is empty");
    CHECK(error_of("expect \"a\"b 5") == "t.nnhs:1: a closing quote must be followed by a space");
    CHECK(error_of("wait \"5\"") == "t.nnhs:1: only TEXT goes in double quotes");
    CHECK(error_of("expect \"" + std::string(129, 'x') + "\" 5") == "t.nnhs:1: TEXT is longer than 128 characters");
    CHECK(error_of("expect \"" + std::string(128, 'x') + "\" 5").empty());
}

NNH_TEST(parser_reports_anything_after_end) {
    const ParsedScript p = parse("end\n# fine\n\nwait 1\nend\n");
    CHECK(p.errors.size() == 2);
    CHECK(p.errors.size() == 2 && p.errors[0] == "t.nnhs:4: nothing may follow end" && p.errors[1] == "t.nnhs:5: nothing may follow end");
}

NNH_TEST(parser_reports_every_error_in_line_order) {
    const ParsedScript p = parse("jump\nwait 1\npress Q\nwait 0\n");
    CHECK(p.errors.size() == 3);
    CHECK(p.errors.size() == 3 && has(p.errors[0], ":1:") && has(p.errors[1], ":3:") && has(p.errors[2], ":4:"));
}

NNH_TEST(parser_enforces_the_size_limits) {
    std::string big;
    while (big.size() < kMaxScriptBytes) big += std::string(kMaxLineChars - 1, '#') + "\n"; // 256 lines
    CHECK(big.size() == kMaxScriptBytes && error_of(big).empty());
    CHECK(error_of(big + "#") == "t.nnhs: the script is larger than 64 KB");
    std::string many;
    for (int i = 0; i < kMaxScriptLines + 1; ++i) many += "\n";
    CHECK(error_of(many) == "t.nnhs:4097: the script has more than 4096 lines");
    std::string max_lines;
    for (int i = 0; i < kMaxScriptLines; ++i) max_lines += "\n";
    CHECK(error_of(max_lines).empty());
    CHECK(error_of("wait 1 #" + std::string(kMaxLineChars - 8, 'x')).empty());
    CHECK(error_of("wait 1 #" + std::string(kMaxLineChars - 7, 'x')) == "t.nnhs:1: the line is longer than 256 characters");
}

NNH_TEST(parser_warns_about_input_right_after_an_expect) {
    const ParsedScript p = parse("expect \"a\" 5\nwait 9\ntap A\ntap B\n");
    CHECK(p.errors.empty() && p.script.warnings.size() == 1);
    CHECK(p.script.warnings.size() == 1 &&
          p.script.warnings[0] == "t.nnhs:3: warning: input 9 frames after the expect on line 1 will probably reach "
                                  "the console late; wait 10 frames or more first");
    CHECK(parse("expect \"a\" 5\nwait 10\ntap A\n").script.warnings.empty());
    CHECK(parse("expect \"a\" 5\nhold A 4\nwait 9\n").script.warnings.size() == 1);
    CHECK(parse("expect \"a\" 5\nscreenshot x\n").script.warnings.size() == 1);
    CHECK(parse("expect \"a\" 5\nfail-on \"b\"\nexpect \"c\" 5\nwait 20\ntap A").script.warnings.empty());
    CHECK(parse("tap A\nwait 1\ntap A\n").script.warnings.empty());
}

NNH_TEST(parser_reads_lines_one_at_a_time_for_interactive_use) {
    ScriptParser parser{.file = "typed"};
    Script script;
    std::vector<std::string> errors;
    parse_script_line(parser, "touch 5 5", 1, script, errors);
    parse_script_line(parser, "drag 9 9 3", 2, script, errors);
    parse_script_line(parser, "press nothing", 3, script, errors);
    CHECK(script.steps.size() == 2 && script.steps[1].command == Command::Drag);
    CHECK(errors.size() == 1 && has(errors[0], "typed:3: unknown button"));
}
